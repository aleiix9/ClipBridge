#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include "src/clipbridge_config.h"
#include "src/st7789_display.h"
#include "src/cst328_touch.h"
#include "src/st25dv_tag.h"
#include "src/clip_storage.h"
#include "src/clipbridge_ui.h"
#include "src/lvgl/lvgl.h"

// -----------------------------------------------------------------------------
// Global hardware / services
// -----------------------------------------------------------------------------
ST7789Display lcd;
CST328Touch touch;
ST25DVTag nfcCopy("COPY", PIN_NFC_COPY_SDA, PIN_NFC_COPY_SCL, PIN_NFC_COPY_GPO);
ST25DVTag nfcPaste("PASTE", PIN_NFC_PASTE_SDA, PIN_NFC_PASTE_SCL, PIN_NFC_PASTE_GPO);
ClipStorage storage;
ClipBridgeUI ui;
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

lv_display_t *lvDisplay = nullptr;
lv_indev_t *lvTouch = nullptr;
uint8_t *lvBuffer = nullptr;

String apSsid;
String copyUrl;
String pasteUrl;

bool touchOk = false;
bool copyNfcOk = false;
bool pasteNfcOk = false;
bool sdOk = false;

uint32_t lastBatteryUpdate = 0;
uint32_t lastBringupStepAt = 0;

// Dedicated visual feedback for the physical COPY and PASTE zones.
// A normal text action lights the corresponding LED for about 3 seconds.
// File transfers hold the LED ON for the whole transfer and keep it lit for
// at least ACTION_LED_MIN_MS afterwards.
bool copyLedHeld = false;
bool pasteLedHeld = false;
uint32_t copyLedOffAt = 0;
uint32_t pasteLedOffAt = 0;

// Display power management. The ESP32 and NFC stay alive; only the LCD
// backlight is switched off after inactivity. Separate pending flags let the
// physical NFC zone also light its matching action LED.
volatile bool nfcCopyWakePending = false;
volatile bool nfcPasteWakePending = false;
bool displayAwake = true;
uint32_t lastDisplayActivity = 0;
float filteredBatteryVoltage = 0.0f;

// -----------------------------------------------------------------------------
// Clipboard history model
// -----------------------------------------------------------------------------
enum class ClipKind : uint8_t {
    Text = 0,
    Link,
    Image,
    File
};

struct ClipItem {
    bool valid = false;
    ClipKind kind = ClipKind::Text;
    String title;
    String text;
    String filePath;
    String mimeType;
    uint32_t fileSize = 0;
};

static constexpr size_t HISTORY_CAPACITY = 50;
ClipItem historyItems[HISTORY_CAPACITY];
size_t historyCount = 0;
int8_t selectedHistoryIndex = -1;
uint8_t historyBrowserIndex = 0;

File activeUploadFile;
String activeUploadName;
String activeUploadPath;
String activeUploadMime;
uint32_t activeUploadBytes = 0;
bool activeUploadOk = false;
String uploadResultMessage;

// -----------------------------------------------------------------------------
// Wi-Fi state
// -----------------------------------------------------------------------------
enum class StationState : uint8_t {
    Idle = 0,
    Connecting,
    Connected,
    Failed
};

StationState stationState = StationState::Idle;
String stationSsid;
String stationPassword;
bool stationSaveCredentials = false;
uint32_t stationConnectStartedAt = 0;

bool wifiScanRunning = false;
String wifiScanSsids[3];
int32_t wifiScanRssi[3] = {0, 0, 0};
bool nfcUrlsNeedUpdate = true;
bool mdnsStarted = false;

// -----------------------------------------------------------------------------
// Deferred bring-up state machine
// -----------------------------------------------------------------------------
enum class BringupStage : uint8_t {
    Start = 0,
    InitSd,
    InitNfcCopy,
    InitNfcPaste,
    ProgramTags,
    Done
};

BringupStage bringupStage = BringupStage::Start;

// Forward declarations.
bool isStationConnected();
void updateServiceUrls(bool programTags);
void refreshUiStatus();
void refreshHistoryUi();
void beginStationConnection(const String &ssid, const String &password, bool saveCredentials);
void onWifiScanRequested();
void onWifiConnectRequested();
void processWifiScanResults(int16_t count);
void serviceWifiScan();
void serviceStationConnection();
void onClearAllRequested();
void onViewAllRequested();
void onBrowserPrev();
void onBrowserNext();
void onBrowserOpen();
void onBrowserSend();
void refreshHistoryBrowser();
void startMdns();

// -----------------------------------------------------------------------------
// General utility
// -----------------------------------------------------------------------------
static uint32_t lvMillis() {
    return millis();
}

String htmlEscape(const String &input) {
    String output;
    output.reserve(input.length() + 32);

    for (size_t i = 0; i < input.length(); ++i) {
        switch (input[i]) {
            case '&': output += F("&amp;"); break;
            case '<': output += F("&lt;"); break;
            case '>': output += F("&gt;"); break;
            case '"': output += F("&quot;"); break;
            case '\'': output += F("&#39;"); break;
            default: output += input[i]; break;
        }
    }
    return output;
}

String jsonEscape(const String &input) {
    String output;
    output.reserve(input.length() + 24);

    for (size_t i = 0; i < input.length(); ++i) {
        const char c = input[i];
        switch (c) {
            case '"': output += F("\\\""); break;
            case '\\': output += F("\\\\"); break;
            case '\b': output += F("\\b"); break;
            case '\f': output += F("\\f"); break;
            case '\n': output += F("\\n"); break;
            case '\r': output += F("\\r"); break;
            case '\t': output += F("\\t"); break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buffer[7];
                    snprintf(buffer, sizeof(buffer), "\\u%04X", static_cast<uint8_t>(c));
                    output += buffer;
                } else {
                    output += c;
                }
                break;
        }
    }
    return output;
}

static void writeActionLed(int pin, bool activeHigh, bool on) {
    digitalWrite(pin, on == activeHigh ? HIGH : LOW);
}

void pulseCopyLed(uint32_t durationMs = ACTION_LED_MIN_MS) {
    writeActionLed(PIN_COPY_LED, COPY_LED_ACTIVE_HIGH, true);
    copyLedOffAt = millis() + durationMs;
}

void pulsePasteLed(uint32_t durationMs = ACTION_LED_MIN_MS) {
    writeActionLed(PIN_PASTE_LED, PASTE_LED_ACTIVE_HIGH, true);
    pasteLedOffAt = millis() + durationMs;
}

void holdCopyLed() {
    copyLedHeld = true;
    writeActionLed(PIN_COPY_LED, COPY_LED_ACTIVE_HIGH, true);
    copyLedOffAt = 0;
}

void releaseCopyLed(uint32_t tailMs = ACTION_LED_MIN_MS) {
    copyLedHeld = false;
    pulseCopyLed(tailMs);
}

void holdPasteLed() {
    pasteLedHeld = true;
    writeActionLed(PIN_PASTE_LED, PASTE_LED_ACTIVE_HIGH, true);
    pasteLedOffAt = 0;
}

void releasePasteLed(uint32_t tailMs = ACTION_LED_MIN_MS) {
    pasteLedHeld = false;
    pulsePasteLed(tailMs);
}

void serviceActionLeds() {
    const uint32_t now = millis();

    if (!copyLedHeld && copyLedOffAt != 0 &&
        static_cast<int32_t>(now - copyLedOffAt) >= 0) {
        writeActionLed(PIN_COPY_LED, COPY_LED_ACTIVE_HIGH, false);
        copyLedOffAt = 0;
    }

    if (!pasteLedHeld && pasteLedOffAt != 0 &&
        static_cast<int32_t>(now - pasteLedOffAt) >= 0) {
        writeActionLed(PIN_PASTE_LED, PASTE_LED_ACTIVE_HIGH, false);
        pasteLedOffAt = 0;
    }
}

uint8_t batteryPercent(float volts) {
    // Approximate 1-cell LiPo state of charge from open-circuit voltage.
    // This is deliberately non-linear; it is substantially more useful than
    // a straight 3.35V..4.18V interpolation for a battery-only product.
    struct Point { float v; uint8_t p; };
    static constexpr Point curve[] = {
        {3.35f, 0}, {3.50f, 5}, {3.60f, 12}, {3.70f, 25},
        {3.75f, 35}, {3.80f, 45}, {3.85f, 55}, {3.90f, 65},
        {3.95f, 75}, {4.00f, 82}, {4.05f, 90}, {4.10f, 95},
        {4.15f, 99}, {4.18f, 100}
    };

    if (volts <= curve[0].v) return curve[0].p;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (volts <= curve[i].v) {
            const float span = curve[i].v - curve[i - 1].v;
            const float t = span > 0.0f ? (volts - curve[i - 1].v) / span : 0.0f;
            const float p = curve[i - 1].p + t * (curve[i].p - curve[i - 1].p);
            return static_cast<uint8_t>(p + 0.5f);
        }
    }
    return 100;
}

float readBatteryVoltage() {
    uint32_t millivolts = 0;
    for (int i = 0; i < 8; ++i) {
        millivolts += analogReadMilliVolts(PIN_VBAT_SENSE);
        delayMicroseconds(250);
    }
    millivolts /= 8U;
    return (static_cast<float>(millivolts) / 1000.0f) * VBAT_DIVIDER_RATIO;
}

void noteDisplayActivity() {
    lastDisplayActivity = millis();
}

void wakeDisplay() {
    noteDisplayActivity();
    if (!displayAwake) {
        lcd.setBacklight(true);
        displayAwake = true;
    }
}

void serviceDisplayPower() {
    bool copyWake = false;
    bool pasteWake = false;

    noInterrupts();
    if (nfcCopyWakePending) {
        copyWake = true;
        nfcCopyWakePending = false;
    }
    if (nfcPasteWakePending) {
        pasteWake = true;
        nfcPasteWakePending = false;
    }
    interrupts();

    if (copyWake || pasteWake) {
        wakeDisplay();
        // Immediate physical feedback as soon as the phone enters an NFC zone.
        if (copyWake) pulseCopyLed();
        if (pasteWake) pulsePasteLed();
    }

    if (displayAwake &&
        static_cast<uint32_t>(millis() - lastDisplayActivity) >= DISPLAY_BACKLIGHT_TIMEOUT_MS) {
        lcd.setBacklight(false);
        displayAwake = false;
    }
}

void IRAM_ATTR onNfcCopyWakeInterrupt() {
    // The ST25DV GPO pulse can be only a few hundred microseconds long, so it
    // is latched here and handled safely from loop().
    nfcCopyWakePending = true;
}

void IRAM_ATTR onNfcPasteWakeInterrupt() {
    nfcPasteWakePending = true;
}

String flattenText(const String &input, size_t maximum) {
    String output = input;
    output.replace("\r", " ");
    output.replace("\n", " ");
    output.trim();
    if (output.length() > maximum) {
        output = output.substring(0, maximum - 3);
        output += "...";
    }
    return output;
}

ClipKind detectTextKind(const String &text) {
    String candidate = text;
    candidate.trim();
    candidate.toLowerCase();

    if (candidate.length() == 0) {
        return ClipKind::Text;
    }

    // URLs copied by browsers normally include a scheme, but some apps copy
    // www.example.com or example.com/path instead. Detect all three cases.
    if (
        candidate.indexOf("http://") >= 0 ||
        candidate.indexOf("https://") >= 0 ||
        candidate.indexOf("ftp://") >= 0 ||
        candidate.startsWith("www.") ||
        candidate.startsWith("mailto:")
    ) {
        return ClipKind::Link;
    }

    // A bare domain/path is considered a link only when the whole clipboard
    // item is a single token. This avoids classifying ordinary sentences that
    // happen to contain a dot as URLs.
    if (
        candidate.indexOf(' ') >= 0 ||
        candidate.indexOf('\n') >= 0 ||
        candidate.indexOf('\r') >= 0 ||
        candidate.indexOf('\t') >= 0
    ) {
        return ClipKind::Text;
    }

    while (
        candidate.endsWith(".") || candidate.endsWith(",") ||
        candidate.endsWith(";") || candidate.endsWith(":") ||
        candidate.endsWith(")") || candidate.endsWith("]")
    ) {
        candidate.remove(candidate.length() - 1);
    }

    String host = candidate;
    int cut = host.indexOf('/');
    if (cut >= 0) host = host.substring(0, cut);
    cut = host.indexOf('?');
    if (cut >= 0) host = host.substring(0, cut);
    cut = host.indexOf('#');
    if (cut >= 0) host = host.substring(0, cut);

    // Remove an optional port from the host.
    const int colon = host.lastIndexOf(':');
    if (colon > 0) {
        bool numeric_port = true;
        for (size_t i = static_cast<size_t>(colon + 1); i < host.length(); ++i) {
            if (!isDigit(host[i])) {
                numeric_port = false;
                break;
            }
        }
        if (numeric_port) host = host.substring(0, colon);
    }

    const int at = host.lastIndexOf('@');
    if (at >= 0 && at + 1 < static_cast<int>(host.length())) {
        host = host.substring(at + 1);
    }

    const int last_dot = host.lastIndexOf('.');
    if (last_dot <= 0 || last_dot >= static_cast<int>(host.length()) - 2) {
        return ClipKind::Text;
    }

    for (size_t i = 0; i < host.length(); ++i) {
        const char c = host[i];
        if (!(isAlphaNumeric(c) || c == '-' || c == '.')) {
            return ClipKind::Text;
        }
    }

    for (size_t i = static_cast<size_t>(last_dot + 1); i < host.length(); ++i) {
        if (!isAlpha(host[i])) {
            return ClipKind::Text;
        }
    }

    return ClipKind::Link;
}

bool isImageFile(const String &filename, const String &mime) {
    String lowerName = filename;
    lowerName.toLowerCase();
    String lowerMime = mime;
    lowerMime.toLowerCase();

    if (lowerMime.startsWith("image/")) return true;
    return
        lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
        lowerName.endsWith(".jpeg") || lowerName.endsWith(".gif") ||
        lowerName.endsWith(".webp") || lowerName.endsWith(".bmp") ||
        lowerName.endsWith(".heic") || lowerName.endsWith(".heif") ||
        lowerName.endsWith(".avif");
}

String clipTypeName(const ClipItem &item) {
    if (item.kind == ClipKind::Link) return "LINK";
    if (item.kind == ClipKind::Image) return "IMAGE";
    if (item.kind == ClipKind::File) {
        String lower = item.title;
        lower.toLowerCase();
        return lower.endsWith(".pdf") ? "PDF" : "FILE";
    }
    return "TEXT";
}

String formatFileSize(uint32_t bytes) {
    if (bytes >= 1024U * 1024U) {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / 1048576.0f);
        return String(buffer);
    }
    if (bytes >= 1024U) {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "%.1f KB", bytes / 1024.0f);
        return String(buffer);
    }
    return String(bytes) + " B";
}

String apiTypeName(const ClipItem &item) {
    if (item.kind == ClipKind::Link) return "link";
    if (item.kind == ClipKind::Image) return "image";
    if (item.kind == ClipKind::File) return "file";
    return "text";
}

String itemPreview(const ClipItem &item, size_t maximum = 140) {
    if (item.kind == ClipKind::Text || item.kind == ClipKind::Link) {
        return flattenText(item.text, maximum);
    }
    return item.title;
}

String sanitizeFilename(const String &filename) {
    String clean;
    clean.reserve(64);
    for (size_t i = 0; i < filename.length() && clean.length() < 55; ++i) {
        const char c = filename[i];
        const bool safe =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_';
        clean += safe ? c : '_';
    }
    if (clean.length() == 0) clean = "uploaded_file.bin";
    return clean;
}

String mimeFromFilename(const String &filename) {
    String lower = filename;
    lower.toLowerCase();
    if (lower.endsWith(".pdf")) return "application/pdf";
    if (lower.endsWith(".png")) return "image/png";
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
    if (lower.endsWith(".gif")) return "image/gif";
    if (lower.endsWith(".webp")) return "image/webp";
    if (lower.endsWith(".bmp")) return "image/bmp";
    if (lower.endsWith(".heic")) return "image/heic";
    if (lower.endsWith(".heif")) return "image/heif";
    if (lower.endsWith(".avif")) return "image/avif";
    if (lower.endsWith(".txt")) return "text/plain";
    if (lower.endsWith(".json")) return "application/json";
    if (lower.endsWith(".zip")) return "application/zip";
    return "application/octet-stream";
}

String historyFieldEncode(const String &input) {
    String out;
    out.reserve(input.length() + 16);
    const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < input.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(input[i]);
        if (c == '%' || c == '|' || c == '\n' || c == '\r') {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

int historyHexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

String historyFieldDecode(const String &input) {
    String out;
    out.reserve(input.length());
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '%' && i + 2 < input.length()) {
            const int hi = historyHexValue(input[i + 1]);
            const int lo = historyHexValue(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += input[i];
    }
    return out;
}

char historyKindCode(ClipKind kind) {
    switch (kind) {
        case ClipKind::Link: return 'L';
        case ClipKind::Image: return 'I';
        case ClipKind::File: return 'F';
        case ClipKind::Text:
        default: return 'T';
    }
}

void writeHistoryRecord(File &file, const ClipItem &item) {
    file.print(historyKindCode(item.kind));
    file.print('|');
    file.print(historyFieldEncode(item.title));
    file.print('|');
    file.print(historyFieldEncode(
        item.kind == ClipKind::Text || item.kind == ClipKind::Link
            ? item.text
            : item.filePath
    ));
    file.print('|');
    file.print(historyFieldEncode(item.mimeType));
    file.print('|');
    file.println(item.fileSize);
}

void rewriteHistoryIndex() {
    if (!sdOk) return;

    SD_MMC.remove("/history_v8.idx");
    File file = SD_MMC.open("/history_v8.idx", FILE_WRITE);
    if (!file) return;

    for (size_t i = historyCount; i > 0; --i) {
        const ClipItem &item = historyItems[i - 1];
        if (item.valid) writeHistoryRecord(file, item);
    }
    file.close();
}

void compactHistoryIndexIfNeeded() {
    if (!sdOk || !SD_MMC.exists("/history_v8.idx")) return;

    File check = SD_MMC.open("/history_v8.idx", FILE_READ);
    if (!check) return;
    const size_t bytes = check.size();
    check.close();

    // Keep boot time bounded even after months of use. The UI and RAM model
    // keep only the newest 50 entries, so older index records are unnecessary.
    if (bytes < 128U * 1024U) return;

    SD_MMC.remove("/history_v8.idx");
    File compacted = SD_MMC.open("/history_v8.idx", FILE_WRITE);
    if (!compacted) return;

    for (size_t i = historyCount; i > 0; --i) {
        const ClipItem &item = historyItems[i - 1];
        if (item.valid) writeHistoryRecord(compacted, item);
    }
    compacted.close();
}

void appendHistoryRecord(const ClipItem &item) {
    if (!sdOk || !item.valid) return;
    File file = SD_MMC.open("/history_v8.idx", FILE_APPEND);
    if (!file) return;
    writeHistoryRecord(file, item);
    file.close();
    compactHistoryIndexIfNeeded();
}

void persistLatestFile(const ClipItem &item) {
    if (!item.valid || (item.kind != ClipKind::File && item.kind != ClipKind::Image)) return;
    preferences.putString("file_name", item.title);
    preferences.putString("file_path", item.filePath);
    preferences.putString("file_mime", item.mimeType);
    preferences.putULong("file_size", item.fileSize);
    preferences.putBool("file_saved", true);
}

void refreshLatestFilePreference() {
    preferences.putBool("file_saved", false);
    preferences.remove("file_name");
    preferences.remove("file_path");
    preferences.remove("file_mime");
    preferences.remove("file_size");

    for (size_t i = 0; i < historyCount; ++i) {
        const ClipItem &item = historyItems[i];
        if (item.valid && (item.kind == ClipKind::File || item.kind == ClipKind::Image)) {
            persistLatestFile(item);
            return;
        }
    }
}

bool insertHistoryItem(const ClipItem &item, bool selectItem) {
    if (!item.valid) return false;

    if (historyCount > 0) {
        const ClipItem &first = historyItems[0];
        const bool itemFileLike = item.kind == ClipKind::File || item.kind == ClipKind::Image;
        const bool firstFileLike = first.kind == ClipKind::File || first.kind == ClipKind::Image;
        const bool sameText =
            !itemFileLike && !firstFileLike && first.kind == item.kind && first.text == item.text;
        const bool sameFile =
            itemFileLike && firstFileLike && first.filePath == item.filePath;
        if (sameText || sameFile) {
            if (selectItem) selectedHistoryIndex = 0;
            return false;
        }
    }

    if (historyCount < HISTORY_CAPACITY) ++historyCount;
    for (size_t i = historyCount - 1; i > 0; --i) {
        historyItems[i] = historyItems[i - 1];
    }
    historyItems[0] = item;

    if (selectItem || selectedHistoryIndex < 0) {
        selectedHistoryIndex = 0;
    } else if (selectedHistoryIndex < static_cast<int8_t>(HISTORY_CAPACITY - 1)) {
        ++selectedHistoryIndex;
    }
    return true;
}

void addTextItem(const String &text, bool saveToStorage, bool selectItem = true) {
    String limited = text;
    limited.trim();
    if (limited.length() == 0) return;
    if (limited.length() > CLIPBOARD_MAX_CHARS) limited = limited.substring(0, CLIPBOARD_MAX_CHARS);

    ClipItem item;
    item.valid = true;
    item.kind = detectTextKind(limited);
    item.text = limited;
    item.title = flattenText(limited, 48);
    const bool inserted = insertHistoryItem(item, selectItem);

    if (saveToStorage && sdOk) {
        storage.saveClipboard(limited);
        if (inserted) appendHistoryRecord(item);
    }
    refreshHistoryUi();
}

void addFileItem(
    const String &filename,
    const String &path,
    const String &mime,
    uint32_t bytes,
    bool selectItem = true,
    bool saveToHistory = true
) {
    ClipItem item;
    item.valid = true;
    item.kind = isImageFile(filename, mime) ? ClipKind::Image : ClipKind::File;
    item.title = filename;
    item.filePath = path;
    item.mimeType = mime;
    item.fileSize = bytes;
    const bool inserted = insertHistoryItem(item, selectItem);
    persistLatestFile(item);
    if (inserted && saveToHistory) appendHistoryRecord(item);
    refreshHistoryUi();
}

const ClipItem *selectedItem() {
    if (selectedHistoryIndex < 0 ||
        selectedHistoryIndex >= static_cast<int8_t>(historyCount)) {
        return nullptr;
    }
    const ClipItem &item = historyItems[selectedHistoryIndex];
    return item.valid ? &item : nullptr;
}

ClipItem *historyItemById(int id) {
    if (id < 0 || id >= static_cast<int>(historyCount)) return nullptr;
    ClipItem &item = historyItems[id];
    return item.valid ? &item : nullptr;
}

bool deleteHistoryItem(size_t index) {
    if (index >= historyCount || !historyItems[index].valid) return false;

    const ClipItem removed = historyItems[index];
    if (sdOk && (removed.kind == ClipKind::File || removed.kind == ClipKind::Image) &&
        removed.filePath.length() > 0 && SD_MMC.exists(removed.filePath.c_str())) {
        SD_MMC.remove(removed.filePath.c_str());
    }

    for (size_t i = index + 1; i < historyCount; ++i) {
        historyItems[i - 1] = historyItems[i];
    }
    if (historyCount > 0) {
        historyItems[historyCount - 1] = ClipItem();
        --historyCount;
    }

    if (historyCount == 0) {
        selectedHistoryIndex = -1;
        historyBrowserIndex = 0;
    } else {
        if (selectedHistoryIndex == static_cast<int8_t>(index)) {
            selectedHistoryIndex = index >= historyCount
                ? static_cast<int8_t>(historyCount - 1)
                : static_cast<int8_t>(index);
        } else if (selectedHistoryIndex > static_cast<int8_t>(index)) {
            --selectedHistoryIndex;
        }
        if (historyBrowserIndex >= historyCount) {
            historyBrowserIndex = static_cast<uint8_t>(historyCount - 1);
        }
    }

    rewriteHistoryIndex();
    refreshLatestFilePreference();
    refreshHistoryUi();
    onHomeButton();
    return true;
}

void refreshHistoryUi() {
    const size_t shownCount = historyCount < ClipBridgeUI::HISTORY_SLOTS
        ? historyCount
        : ClipBridgeUI::HISTORY_SLOTS;

    for (uint8_t i = 0; i < ClipBridgeUI::HISTORY_SLOTS; ++i) {
        if (i >= shownCount || !historyItems[i].valid) {
            ui.clearHistoryItem(i);
            continue;
        }

        const ClipItem &item = historyItems[i];
        const String type = clipTypeName(item);
        String meta;
        if (item.kind == ClipKind::File || item.kind == ClipKind::Image) {
            meta = type + " | " + formatFileSize(item.fileSize);
        } else if (item.kind == ClipKind::Link) {
            meta = "Link | recent";
        } else {
            meta = "Text | recent";
        }

        ui.setHistoryItem(
            i,
            type,
            item.title,
            meta,
            selectedHistoryIndex == static_cast<int8_t>(i)
        );
    }
    ui.setHistoryCount(static_cast<uint8_t>(historyCount));
}

// -----------------------------------------------------------------------------
// Wi-Fi AP + station management
// -----------------------------------------------------------------------------
bool isStationConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String activeServiceBaseUrl() {
    if (isStationConnected() && mdnsStarted) {
        return F("http://clipbridge.local");
    }

    const IPAddress ip = isStationConnected() ? WiFi.localIP() : WiFi.softAPIP();
    return String("http://") + ip.toString();
}

void refreshUiStatus() {
    ui.setHeaderSsid(isStationConnected() ? WiFi.SSID() : apSsid);
}

void updateServiceUrls(bool programTags) {
    const String base = activeServiceBaseUrl();
    const String newCopyUrl = base + COPY_PATH;
    const String newPasteUrl = base + PASTE_PATH;

    if (newCopyUrl != copyUrl || newPasteUrl != pasteUrl) {
        copyUrl = newCopyUrl;
        pasteUrl = newPasteUrl;
        nfcUrlsNeedUpdate = true;
    }

    if (!programTags || !NFC_PROGRAM_URLS_ON_BOOT || !nfcUrlsNeedUpdate) {
        return;
    }

    bool completed = true;
    if (copyNfcOk) {
        copyNfcOk = nfcCopy.ensureUri(copyUrl);
        completed = completed && copyNfcOk;
    }
    if (pasteNfcOk) {
        pasteNfcOk = nfcPaste.ensureUri(pasteUrl);
        completed = completed && pasteNfcOk;
    }

    if (completed) {
        nfcUrlsNeedUpdate = false;
    }

    Serial.printf("COPY NFC URL: %s\n", copyUrl.c_str());
    Serial.printf("PASTE NFC URL: %s\n", pasteUrl.c_str());
    refreshUiStatus();
}

void beginStationConnection(
    const String &ssid,
    const String &password,
    bool saveCredentials
) {
    if (ssid.length() == 0) {
        ui.setWifiStatus("Select a network first");
        return;
    }

    stationSsid = ssid;
    stationPassword = password;
    stationSaveCredentials = saveCredentials;
    stationConnectStartedAt = millis();
    stationState = StationState::Connecting;

    ui.setWifiStatus(String("Connecting to ") + ssid + "...");
    ui.showToast(String("Connecting to ") + ssid);

    WiFi.disconnect(false, false);
    WiFi.begin(
        stationSsid.c_str(),
        stationPassword.length() > 0 ? stationPassword.c_str() : nullptr
    );
}

void onWifiScanRequested() {
    if (wifiScanRunning) {
        ui.setWifiStatus("Scan already running...");
        return;
    }

    WiFi.scanDelete();
    ui.setWifiStatus("Scanning nearby networks...");
    const int16_t result = WiFi.scanNetworks(true, true);

    if (result >= 0) {
        wifiScanRunning = false;
        processWifiScanResults(result);
    } else if (result == -1) {
        wifiScanRunning = true;
    } else {
        wifiScanRunning = false;
        ui.setWifiStatus("Wi-Fi scan could not start");
    }
}

void onWifiConnectRequested() {
    beginStationConnection(ui.selectedWifiSsid(), ui.wifiPassword(), true);
}

void processWifiScanResults(int16_t count) {
    size_t found = 0;
    for (int16_t i = 0; i < count && found < 3; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        bool duplicate = false;
        for (size_t j = 0; j < found; ++j) {
            if (wifiScanSsids[j] == ssid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        wifiScanSsids[found] = ssid;
        wifiScanRssi[found] = WiFi.RSSI(i);
        ++found;
    }

    for (size_t i = found; i < 3; ++i) {
        wifiScanSsids[i] = "";
        wifiScanRssi[i] = 0;
    }

    ui.setWifiScanResults(wifiScanSsids, wifiScanRssi, found);
    ui.setWifiStatus(found > 0 ? "Tap network, then type password" : "No networks found");
    WiFi.scanDelete();
}

void serviceWifiScan() {
    if (!wifiScanRunning) return;
    const int16_t result = WiFi.scanComplete();
    if (result >= 0) {
        wifiScanRunning = false;
        processWifiScanResults(result);
    } else if (result == -2) {
        wifiScanRunning = false;
        ui.setWifiStatus("Wi-Fi scan failed");
    }
}

void serviceStationConnection() {
    if (stationState == StationState::Connecting) {
        if (isStationConnected()) {
            stationState = StationState::Connected;

            if (stationSaveCredentials) {
                preferences.putString("ssid", stationSsid);
                preferences.putString("pass", stationPassword);
                preferences.putBool("wifi_saved", true);
            }

            const String ip = WiFi.localIP().toString();
            ui.setWifiStatus(
                stationSaveCredentials
                    ? String("Saved. Auto-connect enabled\n") + stationSsid + "  " + ip
                    : String("Connected: ") + stationSsid + "  " + ip
            );
            ui.setHeaderSsid(stationSsid);
            ui.clearWifiPassword();
            ui.showToast(String("Wi-Fi connected: ") + ip);

            startMdns();
            nfcUrlsNeedUpdate = true;
            updateServiceUrls(bringupStage == BringupStage::Done);
        } else if (millis() - stationConnectStartedAt > 18000U) {
            stationState = StationState::Failed;
            WiFi.disconnect(false, false);
            ui.setWifiStatus("Connection failed. Check password.");
            ui.setHeaderSsid(apSsid);
            ui.showToast("Connection failed. ClipBridge AP stays active.");
            nfcUrlsNeedUpdate = true;
            updateServiceUrls(bringupStage == BringupStage::Done);
        }
    } else if (stationState == StationState::Connected && !isStationConnected()) {
        stationState = StationState::Connecting;
        stationConnectStartedAt = millis();
        ui.setWifiStatus("Wi-Fi lost. Reconnecting...");
        WiFi.reconnect();
    }
}

// -----------------------------------------------------------------------------
// LVGL bridge and touch filtering
// -----------------------------------------------------------------------------
void lvFlushCallback(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap) {
    lcd.flush(area, pixelMap);
    lv_display_flush_ready(display);
}

void lvTouchReadCallback(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;

    static bool stablePressed = false;
    static bool wakeTouchLock = false;
    static uint16_t lastX = 0;
    static uint16_t lastY = 0;
    static uint32_t lastRawTouchAt = 0;

    uint16_t x = 0;
    uint16_t y = 0;
    const uint32_t now = millis();
    const bool rawPressed = touch.readPoint(x, y);

    // First contact while the backlight is off only wakes the display. This
    // prevents the wake gesture from accidentally activating a UI control.
    if (rawPressed && !displayAwake) {
        lastX = x;
        lastY = y;
        lastRawTouchAt = now;
        stablePressed = false;
        wakeTouchLock = true;
        wakeDisplay();
        data->point.x = static_cast<int32_t>(lastX);
        data->point.y = static_cast<int32_t>(lastY);
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (wakeTouchLock) {
        if (rawPressed) {
            lastX = x;
            lastY = y;
            lastRawTouchAt = now;
            noteDisplayActivity();
        } else if (static_cast<uint32_t>(now - lastRawTouchAt) > TOUCH_RELEASE_DEBOUNCE_MS) {
            wakeTouchLock = false;
        }

        data->point.x = static_cast<int32_t>(lastX);
        data->point.y = static_cast<int32_t>(lastY);
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (rawPressed) {
        lastX = x;
        lastY = y;
        lastRawTouchAt = now;
        stablePressed = true;
        noteDisplayActivity();
    } else if (
        stablePressed &&
        static_cast<uint32_t>(now - lastRawTouchAt) > TOUCH_RELEASE_DEBOUNCE_MS
    ) {
        stablePressed = false;
    }

    data->point.x = static_cast<int32_t>(lastX);
    data->point.y = static_cast<int32_t>(lastY);
    data->state = stablePressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}


// -----------------------------------------------------------------------------
// Screen callbacks
// -----------------------------------------------------------------------------
void describeItem(
    const ClipItem &item,
    String &type,
    String &title,
    String &preview,
    String &details
) {
    type = clipTypeName(item);

    if (item.kind == ClipKind::Image) {
        title = item.title;
        String lower = item.title;
        lower.toLowerCase();
        if (lower.endsWith(".heic") || lower.endsWith(".heif")) {
            preview = "HEIC/HEIF image stored on microSD. It will be delivered as the original file.";
        } else {
            preview = "Image stored on microSD and ready for COPY download.";
        }
        details = "IMAGE | " + formatFileSize(item.fileSize);
    } else if (item.kind == ClipKind::File) {
        title = item.title;
        preview = type == "PDF"
            ? "PDF stored on microSD and ready for COPY download."
            : "File stored on microSD and ready for COPY download.";
        details = type + " | " + formatFileSize(item.fileSize);
    } else if (item.kind == ClipKind::Link) {
        title = "Ready";
        preview = flattenText(item.text, 170);
        details = "LINK | " + String(item.text.length()) + " characters";
    } else {
        title = "Ready";
        preview = flattenText(item.text, 170);
        details = "TEXT | " + String(item.text.length()) + " characters";
    }
}

void onHomeButton() {
    const ClipItem *item = selectedItem();
    if (item == nullptr) {
        ui.showSelectedItem("NONE", "No item selected", "", "", false);
        return;
    }

    String type;
    String title;
    String preview;
    String details;
    describeItem(*item, type, title, preview, details);
    ui.showSelectedItem(type, title, preview, details, true);
}

void onSettingsButton() {
    ui.showWifiSetup();
    onWifiScanRequested();
}

void selectHistory(uint8_t index) {
    if (index >= historyCount || !historyItems[index].valid) return;
    selectedHistoryIndex = static_cast<int8_t>(index);
    ui.setSelectedHistory(index);
    onHomeButton();
}

void openHistory(uint8_t index) {
    if (index >= historyCount || !historyItems[index].valid) return;
    const ClipItem &item = historyItems[index];
    if (item.kind != ClipKind::Text) {
        ui.showToast("Open is available only for text items.");
        return;
    }
    ui.showTextPreview("TEXT", item.text);
}

void sendHistory(uint8_t index) {
    if (index >= historyCount || !historyItems[index].valid) return;
    selectedHistoryIndex = static_cast<int8_t>(index);
    ui.setSelectedHistory(index);
    ui.showToast("Selected. Tap the COPY zone with your phone.");
}

void refreshHistoryBrowser() {
    if (historyCount == 0) {
        ui.showHistoryBrowser("NONE", "No history item", "", "", 0, 0, false, false);
        return;
    }

    if (historyBrowserIndex >= historyCount) historyBrowserIndex = 0;
    const ClipItem &item = historyItems[historyBrowserIndex];
    String type;
    String title;
    String preview;
    String details;
    describeItem(item, type, title, preview, details);

    ui.showHistoryBrowser(
        type,
        title,
        preview,
        details,
        static_cast<uint8_t>(historyBrowserIndex + 1),
        static_cast<uint8_t>(historyCount),
        item.kind == ClipKind::Text,
        true
    );
}

void onViewAllRequested() {
    historyBrowserIndex =
        selectedHistoryIndex >= 0 && selectedHistoryIndex < static_cast<int8_t>(historyCount)
            ? static_cast<uint8_t>(selectedHistoryIndex)
            : 0;
    refreshHistoryBrowser();
}

void onBrowserPrev() {
    if (historyCount == 0) return;
    historyBrowserIndex = historyBrowserIndex == 0
        ? static_cast<uint8_t>(historyCount - 1)
        : static_cast<uint8_t>(historyBrowserIndex - 1);
    refreshHistoryBrowser();
}

void onBrowserNext() {
    if (historyCount == 0) return;
    historyBrowserIndex = static_cast<uint8_t>((historyBrowserIndex + 1) % historyCount);
    refreshHistoryBrowser();
}

void onBrowserOpen() {
    if (historyCount == 0 || historyBrowserIndex >= historyCount) return;
    const ClipItem &item = historyItems[historyBrowserIndex];
    if (item.kind != ClipKind::Text) {
        ui.showToast("Open is available only for text items.");
        return;
    }
    ui.showTextPreview("TEXT", item.text);
}

void onBrowserSend() {
    if (historyCount == 0 || historyBrowserIndex >= historyCount) return;
    selectedHistoryIndex = static_cast<int8_t>(historyBrowserIndex);
    refreshHistoryUi();
    ui.showToast("Selected. Tap the COPY zone with your phone.");
}

void clearUploadDirectory() {
    if (!sdOk || !SD_MMC.exists("/uploads")) return;
    File dir = SD_MMC.open("/uploads");
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        String path = entry.path();
        const bool directory = entry.isDirectory();
        entry.close();
        if (!directory) SD_MMC.remove(path.c_str());
        entry = dir.openNextFile();
    }
    dir.close();
    SD_MMC.rmdir("/uploads");
}

void onClearAllRequested() {
    if (activeUploadFile) activeUploadFile.close();

    if (sdOk) {
        clearUploadDirectory();
        SD_MMC.remove("/history_v8.idx");
        SD_MMC.remove("/history.txt");
        SD_MMC.remove("/clip.txt");
    }

    for (size_t i = 0; i < HISTORY_CAPACITY; ++i) {
        historyItems[i] = ClipItem();
    }
    historyCount = 0;
    selectedHistoryIndex = -1;
    historyBrowserIndex = 0;

    preferences.putBool("file_saved", false);
    preferences.remove("file_name");
    preferences.remove("file_path");
    preferences.remove("file_mime");
    preferences.remove("file_size");

    refreshHistoryUi();
    ui.showToast("History and uploaded files deleted.");
    ui.showSelectedItem("NONE", "No item selected", "", "", false);
}

// -----------------------------------------------------------------------------
// Web application
// -----------------------------------------------------------------------------
String pageStart(const String &title, const String &accent) {
    String html;
    html.reserve(4200);
    html += F("<!doctype html><html><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'>");
    html += F("<title>");
    html += htmlEscape(title);
    html += F("</title><style>");
    html += F(
        ":root{--accent:"
    );
    html += accent;
    html += F(
        ";--text:#111827;--muted:#667085;--border:#d8e0ea;--bg:#f4f7fb}"
        "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);"
        "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}"
        ".wrap{max-width:620px;margin:auto;padding:16px}"
        ".top{display:flex;align-items:center;justify-content:space-between;background:#fff;"
        "border:1px solid var(--border);border-radius:16px;padding:12px 14px;margin-bottom:14px}"
        ".net{font-size:13px;color:var(--muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".tag{font-weight:800;color:var(--accent)}"
        ".card{background:#fff;border:1px solid var(--border);border-radius:18px;padding:18px;margin:12px 0}"
        "h1{font-size:30px;margin:0 0 6px}h2{font-size:19px;margin:0 0 8px}"
        "p{line-height:1.45;color:var(--muted)}"
        "textarea{width:100%;min-height:170px;border:1px solid var(--border);border-radius:14px;"
        "padding:13px;font-size:16px;color:var(--text);background:#fbfdff;resize:vertical}"
        "button,.button{display:block;width:100%;border:0;border-radius:14px;padding:14px 16px;"
        "font-size:17px;font-weight:800;text-align:center;text-decoration:none;margin-top:10px;cursor:pointer}"
        ".primary{background:var(--accent);color:#fff}.secondary{background:#eef3f9;color:#24324a}"
        ".status{min-height:22px;margin-top:10px;color:var(--muted);font-size:14px}"
        ".file{padding:14px;background:#f8fbff;border:1px solid var(--border);border-radius:14px;word-break:break-word}"
        ".small{font-size:13px;color:var(--muted)}input[type=file]{width:100%;padding:12px;border:1px dashed var(--border);border-radius:12px}"
    );
    html += F("</style></head><body><div class='wrap'><div class='top'><div class='net'>");
    html += htmlEscape(isStationConnected() ? WiFi.SSID() : apSsid);
    html += F("</div><div class='tag'>");
    html += htmlEscape(title);
    html += F("</div></div>");
    return html;
}

String pageEnd() {
    String html = F("<div class='card small'>");
    if (isStationConnected()) {
        html += "Phone and ClipBridge must be on the same Wi-Fi. Device IP: ";
        html += WiFi.localIP().toString();
    } else {
        html += "Fallback mode: connect the phone to ";
        html += htmlEscape(apSsid);
        html += " using password ";
        html += WIFI_AP_PASSWORD;
        html += ".";
    }
    html += F("</div></div></body></html>");
    return html;
}

void handleRoot() {
    String html = pageStart("ClipBridge", "#1664ea");
    html += F("<div class='card'><h1>ClipBridge</h1><p><strong>by Aleix Ferrer. No app needed.</strong></p><p>COPY TO PHONE gets the selected item onto this phone. COPY INSIDE sends text, an image or a file into ClipBridge.</p>");
    html += F("<a class='button primary' href='/copy'>COPY TO PHONE</a>");
    html += F("<a class='button secondary' href='/paste'>COPY INSIDE</a></div>");
    html += pageEnd();
    server.send(200, "text/html; charset=utf-8", html);
}

void handleCopyGet() {
    pulseCopyLed();
    const ClipItem *item = selectedItem();
    String html = pageStart("COPY", "#1664ea");
    html += F("<div class='card'><h1>COPY TO PHONE</h1><p>Get the item selected on ClipBridge onto this phone.</p>");

    if (item == nullptr) {
        html += F("<div class='file'>No item is selected.</div>");
    } else if (item->kind == ClipKind::File || item->kind == ClipKind::Image) {
        html += F("<div class='file'><strong>");
        html += htmlEscape(item->title);
        html += F("</strong><br><span class='small'>");
        html += clipTypeName(*item);
        html += " | ";
        html += formatFileSize(item->fileSize);
        html += F("</span></div>");

        if (item->kind == ClipKind::Image) {
            html += F(
                "<div id='imageBox' style='margin-top:12px;text-align:center'>"
                "<img src='/media' alt='Selected image' "
                "style='max-width:100%;max-height:360px;border-radius:14px;border:1px solid #d8e0ea' "
                "onerror=\"this.style.display='none';document.getElementById('imageNote').style.display='block'\">"
                "<p id='imageNote' class='small' style='display:none'>Preview is not supported by this browser. The original image can still be downloaded.</p>"
                "</div>"
            );
        }

        html += F("<a class='button primary' href='/download'>Download original file</a>");
        html += F("<p class='small'>Images, PDFs and arbitrary files are transferred as files. HEIC/HEIF is kept in its original format.</p>");
    } else {
        html += F("<textarea id='copyText' readonly>");
        html += htmlEscape(item->text);
        html += F("</textarea><button class='primary' onclick='copySelected()'>Copy to this phone</button>");
        html += F("<div id='status' class='status'></div>");
        html += F(
            "<script>async function copySelected(){fetch('/api/copy-activity',{method:'POST',cache:'no-store'}).catch(()=>{});const t=document.getElementById('copyText');const s=document.getElementById('status');"
            "try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(t.value);s.textContent='Copied to clipboard.';return;}}catch(e){}"
            "try{t.focus();t.select();t.setSelectionRange(0,999999);const ok=document.execCommand('copy');"
            "s.textContent=ok?'Copied to clipboard.':'Text selected. Choose Copy from the browser menu.';}"
            "catch(e){t.focus();t.select();s.textContent='Text selected. Choose Copy from the browser menu.';}}"
            "</script>"
        );
    }

    html += F("</div>");
    html += pageEnd();
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.send(200, "text/html; charset=utf-8", html);
}

void handleCopyActivity() {
    pulseCopyLed();
    server.send(204, "text/plain", "");
}

void handlePasteGet() {
    pulsePasteLed();
    String html = pageStart("COPY INSIDE", "#0f9d6e");
    html += F(
        "<div class='card'><h1>COPY INSIDE</h1>"
        "<p>Send text, an image or a file from this phone into ClipBridge.</p>"
        "<form id='textForm' method='post' action='/paste'>"
        "<textarea id='pasteText' name='text' maxlength='1200' "
        "placeholder='Tap here and choose Paste'></textarea>"
        "<button class='primary' type='button' id='pasteButton' onclick='pasteClipboard()'>Try automatic paste</button>"
        "<button class='secondary' type='submit'>Send to ClipBridge</button>"
        "</form><div id='status' class='status'>"
        "On some phones, direct clipboard reading is blocked. Manual Paste is saved automatically."
        "</div></div>"
        "<div class='card'><h2>Image, PDF or other file</h2>"
        "<p>Select an image or file from the phone. JPEG, PNG, WebP, HEIC/HEIF, PDF and other files are stored on microSD.</p>"
        "<form method='post' action='/upload' enctype='multipart/form-data'>"
        "<input type='file' name='file' accept='image/*,.heic,.heif,.pdf,*/*'>"
        "<button class='primary' type='submit'>Upload to ClipBridge</button></form></div>"
        "<script>"
        "const a=document.getElementById('pasteText');"
        "const s=document.getElementById('status');"
        "const b=document.getElementById('pasteButton');"
        "let sending=false;"
        "async function saveText(t){"
        "t=(t||'').trim();"
        "if(!t){s.textContent='No text detected yet.';return false;}"
        "if(sending)return false;"
        "sending=true;b.disabled=true;s.textContent='Saving in ClipBridge...';"
        "try{const body=new URLSearchParams();body.set('text',t);"
        "const r=await fetch('/paste',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});"
        "if(!r.ok)throw new Error('HTTP '+r.status);"
        "s.textContent='Saved in ClipBridge.';b.textContent='Saved';return true;}"
        "catch(e){s.textContent='Could not save. Press Send to ClipBridge.';return false;}"
        "finally{sending=false;b.disabled=false;setTimeout(()=>{b.textContent='Try automatic paste';},1400);}}"
        "function manualPasteMode(){"
        "a.focus();a.scrollIntoView({behavior:'smooth',block:'center'});"
        "s.textContent='Tap and hold inside the box, choose Paste. It will save automatically.';"
        "}"
        "async function pasteClipboard(){"
        "b.textContent='Reading...';s.textContent='Trying to read the phone clipboard...';a.focus();"
        "if(navigator.clipboard&&window.isSecureContext){"
        "try{const read=navigator.clipboard.readText();"
        "const timeout=new Promise((_,rej)=>setTimeout(()=>rej(new Error('timeout')),2500));"
        "const t=await Promise.race([read,timeout]);"
        "if(t){a.value=t;await saveText(t);return;}"
        "s.textContent='Clipboard contains no readable text.';}"
        "catch(e){manualPasteMode();}"
        "}else{manualPasteMode();}"
        "b.textContent='Try automatic paste';"
        "}"
        "a.addEventListener('paste',()=>{"
        "s.textContent='Paste detected. Saving...';"
        "setTimeout(()=>{saveText(a.value);},80);"
        "});"
        "document.getElementById('textForm').addEventListener('submit',()=>{"
        "s.textContent='Saving...';"
        "});"
        "</script>"
    );
    html += pageEnd();
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.send(200, "text/html; charset=utf-8", html);
}

void handlePastePost() {
    if (!server.hasArg("text")) {
        server.send(400, "text/plain", "Missing text");
        return;
    }

    String text = server.arg("text");
    text.trim();
    if (text.length() == 0) {
        server.send(400, "text/plain", "Clipboard text is empty");
        return;
    }

    addTextItem(text, true, true);
    pulsePasteLed();
    ui.showToast("Text received from phone.");

    if (server.header("Accept").indexOf("text/html") >= 0) {
        server.sendHeader("Location", "/paste");
        server.send(303, "text/plain", "");
    } else {
        server.send(200, "application/json", "{\"ok\":true}");
    }
}

void handleUploadData() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        holdPasteLed();
        activeUploadOk = false;
        uploadResultMessage = "";
        activeUploadBytes = 0;
        activeUploadName = sanitizeFilename(upload.filename);
        activeUploadMime = upload.type.length() > 0 ? upload.type : mimeFromFilename(activeUploadName);

        if (!sdOk) {
            uploadResultMessage = "No microSD is mounted.";
            return;
        }

        SD_MMC.mkdir("/uploads");
        char uploadPrefix[16];
        snprintf(
            uploadPrefix,
            sizeof(uploadPrefix),
            "%08lX",
            static_cast<unsigned long>(millis())
        );
        activeUploadPath = String("/uploads/") + uploadPrefix + "_" + activeUploadName;
        for (uint8_t attempt = 1; SD_MMC.exists(activeUploadPath.c_str()) && attempt < 20; ++attempt) {
            activeUploadPath = String("/uploads/") + uploadPrefix + "_" + String(attempt) + "_" + activeUploadName;
        }
        activeUploadFile = SD_MMC.open(activeUploadPath.c_str(), FILE_WRITE);
        if (!activeUploadFile) {
            uploadResultMessage = "Could not create the destination file.";
            return;
        }
        activeUploadOk = true;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (activeUploadOk && activeUploadFile) {
            const size_t written = activeUploadFile.write(upload.buf, upload.currentSize);
            activeUploadBytes += static_cast<uint32_t>(written);
            if (written != upload.currentSize) {
                activeUploadOk = false;
                uploadResultMessage = "microSD write failed.";
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (activeUploadFile) {
            activeUploadFile.close();
        }
        if (activeUploadOk) {
            addFileItem(
                activeUploadName,
                activeUploadPath,
                activeUploadMime,
                activeUploadBytes,
                true
            );
            const bool image = isImageFile(activeUploadName, activeUploadMime);
            uploadResultMessage = image
                ? "Image received. Ready to COPY TO PHONE."
                : "File received. Ready to COPY TO PHONE.";
            ui.showToast(image
                ? "Image received. Tap the COPY zone to get it on a phone."
                : "File received. Tap the COPY zone to get it on a phone.");
        }
        releasePasteLed();
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (activeUploadFile) {
            activeUploadFile.close();
        }
        activeUploadOk = false;
        uploadResultMessage = "Upload aborted.";
        releasePasteLed();
    }
}

void handleUploadComplete() {
    if (server.uri() == "/api/upload") {
        String json = "{\"ok\":";
        json += activeUploadOk ? "true" : "false";
        json += ",\"message\":\"";
        json += jsonEscape(uploadResultMessage.length() > 0 ? uploadResultMessage : String("No file was received."));
        json += "\"}";
        server.send(activeUploadOk ? 200 : 500, "application/json", json);
        return;
    }

    String html = pageStart("COPY INSIDE", "#0f9d6e");
    html += F("<div class='card'><h1>Upload result</h1><p>");
    html += htmlEscape(uploadResultMessage.length() > 0 ? uploadResultMessage : String("No file was received."));
    html += F("</p><a class='button primary' href='/paste'>Back to COPY INSIDE</a></div>");
    html += pageEnd();
    server.send(activeUploadOk ? 200 : 500, "text/html; charset=utf-8", html);
}

void handleDownload() {
    const ClipItem *item = selectedItem();
    if (item == nullptr || (item->kind != ClipKind::File && item->kind != ClipKind::Image) || !sdOk) {
        server.send(404, "text/plain", "Selected file not available");
        return;
    }
    if (!SD_MMC.exists(item->filePath.c_str())) {
        server.send(404, "text/plain", "File missing from microSD");
        return;
    }

    File file = SD_MMC.open(item->filePath.c_str(), FILE_READ);
    if (!file) {
        server.send(500, "text/plain", "Could not open file");
        return;
    }

    server.sendHeader(
        "Content-Disposition",
        String("attachment; filename=\"") + item->title + "\""
    );
    holdCopyLed();
    server.streamFile(file, item->mimeType.length() > 0 ? item->mimeType : "application/octet-stream");
    file.close();
    releaseCopyLed();
}


void handleMedia() {
    const ClipItem *item = selectedItem();
    if (item == nullptr || item->kind != ClipKind::Image || !sdOk) {
        server.send(404, "text/plain", "Selected image not available");
        return;
    }
    if (!SD_MMC.exists(item->filePath.c_str())) {
        server.send(404, "text/plain", "Image missing from microSD");
        return;
    }

    File file = SD_MMC.open(item->filePath.c_str(), FILE_READ);
    if (!file) {
        server.send(500, "text/plain", "Could not open image");
        return;
    }

    server.sendHeader("Content-Disposition", "inline");
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.streamFile(file, item->mimeType.length() > 0 ? item->mimeType : "application/octet-stream");
    file.close();
}

void handleWifiGet() {
    String html = pageStart("Wi-Fi", "#1664ea");
    html += F("<div class='card'><h1>Wi-Fi setup</h1>");
    if (isStationConnected()) {
        html += F("<p>Connected to <strong>");
        html += htmlEscape(WiFi.SSID());
        html += F("</strong> at ");
        html += WiFi.localIP().toString();
        html += F(".</p>");
    }
    html += F(
        "<form method='post' action='/wifi'><input name='ssid' maxlength='32' placeholder='SSID' style='width:100%;padding:13px;border:1px solid #d8e0ea;border-radius:12px;font-size:16px'>"
        "<input name='pass' type='password' maxlength='63' placeholder='Password' style='width:100%;padding:13px;border:1px solid #d8e0ea;border-radius:12px;font-size:16px;margin-top:10px'>"
        "<button class='primary' type='submit'>Connect and save</button></form></div>"
    );
    html += pageEnd();
    server.send(200, "text/html; charset=utf-8", html);
}

void handleWifiPost() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "Missing SSID");
        return;
    }
    beginStationConnection(
        server.arg("ssid"),
        server.hasArg("pass") ? server.arg("pass") : String(),
        true
    );
    server.sendHeader("Location", "/wifi");
    server.send(303, "text/plain", "");
}

String apiItemJson(size_t id, const ClipItem &item, bool includeContent) {
    String json;
    json.reserve(includeContent ? item.text.length() + 360 : 280);
    json += F("{\"id\":");
    json += id;
    json += F(",\"type\":\"");
    json += apiTypeName(item);
    json += F("\",\"title\":\"");
    json += jsonEscape(item.title.length() > 0 ? item.title : itemPreview(item, 48));
    json += F("\",\"preview\":\"");
    json += jsonEscape(itemPreview(item));
    json += F("\",\"size\":");
    json += (item.kind == ClipKind::Text || item.kind == ClipKind::Link)
        ? item.text.length()
        : item.fileSize;
    json += F(",\"selected\":");
    json += selectedHistoryIndex == static_cast<int8_t>(id) ? "true" : "false";

    if (item.mimeType.length() > 0) {
        json += F(",\"mime\":\"");
        json += jsonEscape(item.mimeType);
        json += F("\"");
    }

    if (includeContent && (item.kind == ClipKind::Text || item.kind == ClipKind::Link)) {
        json += F(",\"text\":\"");
        json += jsonEscape(item.text);
        json += F("\"");
    }

    json += F("}");
    return json;
}

bool extractJsonTextField(const String &body, String &text) {
    const int key = body.indexOf("\"text\"");
    if (key < 0) return false;
    const int colon = body.indexOf(':', key + 6);
    if (colon < 0) return false;
    int quote = colon + 1;
    while (quote < static_cast<int>(body.length()) && isSpace(body[quote])) ++quote;
    if (quote >= static_cast<int>(body.length()) || body[quote] != '"') return false;

    String out;
    bool escaped = false;
    for (int i = quote + 1; i < static_cast<int>(body.length()); ++i) {
        const char c = body[i];
        if (escaped) {
            switch (c) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                default: out += c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            text = out;
            return true;
        } else {
            out += c;
        }
    }
    return false;
}

void handleApiStatus() {
    const ClipItem *item = selectedItem();
    String json;
    json.reserve(520);
    json += F("{\"ok\":true,\"device\":\"ClipBridge\",\"hostname\":\"clipbridge\",\"version\":\"12\"");
    json += F(",\"ap_ssid\":\"");
    json += jsonEscape(apSsid);
    json += F("\",\"ap_ip\":\"");
    json += WiFi.softAPIP().toString();
    json += F("\",\"sta_connected\":");
    json += isStationConnected() ? "true" : "false";
    json += F(",\"sta_ssid\":\"");
    json += isStationConnected() ? jsonEscape(WiFi.SSID()) : "";
    json += F("\",\"sta_ip\":\"");
    json += isStationConnected() ? WiFi.localIP().toString() : "";
    json += F("\",\"sd\":");
    json += sdOk ? "true" : "false";
    json += F(",\"battery_percent\":");
    json += filteredBatteryVoltage >= VBAT_VALID_MIN_V && filteredBatteryVoltage <= VBAT_VALID_MAX_V
        ? batteryPercent(filteredBatteryVoltage)
        : BATTERY_FALLBACK_PERCENT;
    json += F(",\"selected_id\":");
    json += selectedHistoryIndex;
    json += F(",\"item_count\":");
    json += historyCount;
    json += F(",\"selected_type\":\"");
    json += item == nullptr ? "none" : clipTypeName(*item);
    json += "\",\"wifi\":\"";
    json += isStationConnected() ? WiFi.SSID() : apSsid;
    json += "\",\"touch\":";
    json += touchOk ? "true" : "false";
    json += ",\"nfc_copy\":";
    json += copyNfcOk ? "true" : "false";
    json += ",\"nfc_paste\":";
    json += pasteNfcOk ? "true" : "false";
    json += "}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

void handleApiItems() {
    String json;
    json.reserve(80 + historyCount * 220);
    json += F("{\"items\":[");
    bool first = true;
    for (size_t i = 0; i < historyCount; ++i) {
        if (!historyItems[i].valid) continue;
        if (!first) json += F(",");
        first = false;
        json += apiItemJson(i, historyItems[i], false);
    }
    json += F("]}");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

void handleApiItem(size_t id) {
    ClipItem *item = historyItemById(static_cast<int>(id));
    if (item == nullptr) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":\"invalid_id\"}");
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", apiItemJson(id, *item, true));
}

void handleApiDownload(size_t id) {
    ClipItem *item = historyItemById(static_cast<int>(id));
    if (item == nullptr || (item->kind != ClipKind::File && item->kind != ClipKind::Image) || !sdOk) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":\"file_not_available\"}");
        return;
    }
    if (!SD_MMC.exists(item->filePath.c_str())) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":\"file_missing\"}");
        return;
    }

    File file = SD_MMC.open(item->filePath.c_str(), FILE_READ);
    if (!file) {
        server.send(500, "application/json", "{\"ok\":false,\"error\":\"open_failed\"}");
        return;
    }

    server.sendHeader(
        "Content-Disposition",
        String("attachment; filename=\"") + item->title + "\""
    );
    server.sendHeader("Content-Length", String(file.size()));
    holdCopyLed();
    server.streamFile(file, item->mimeType.length() > 0 ? item->mimeType : "application/octet-stream");
    file.close();
    releaseCopyLed();
}

void handleApiText() {
    String text;
    if (server.hasArg("plain")) {
        extractJsonTextField(server.arg("plain"), text);
    }
    text.trim();
    if (text.length() == 0) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_text\"}");
        return;
    }

    addTextItem(text, true, true);
    pulsePasteLed();
    ui.showToast("Text received from desktop.");
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiDelete(size_t id) {
    if (!deleteHistoryItem(id)) {
        server.send(404, "application/json", "{\"ok\":false,\"error\":\"invalid_id\"}");
        return;
    }
    pulsePasteLed();
    ui.showToast("Item deleted.");
    server.send(200, "application/json", "{\"ok\":true}");
}

bool parseApiItemPath(const String &uri, size_t &id, bool &download) {
    const String prefix = "/api/items/";
    if (!uri.startsWith(prefix)) return false;

    String rest = uri.substring(prefix.length());
    download = false;
    if (rest.endsWith("/download")) {
        download = true;
        rest = rest.substring(0, rest.length() - 9);
    }
    if (rest.length() == 0) return false;

    for (size_t i = 0; i < rest.length(); ++i) {
        if (!isDigit(rest[i])) return false;
    }
    id = static_cast<size_t>(rest.toInt());
    return true;
}

bool handleApiDynamicRoute() {
    size_t id = 0;
    bool download = false;
    const String uri = server.uri();
    if (!parseApiItemPath(uri, id, download)) return false;

    if (download && server.method() == HTTP_GET) {
        handleApiDownload(id);
        return true;
    }
    if (!download && server.method() == HTTP_GET) {
        handleApiItem(id);
        return true;
    }
    if (!download && server.method() == HTTP_DELETE) {
        handleApiDelete(id);
        return true;
    }

    server.send(405, "application/json", "{\"ok\":false,\"error\":\"method_not_allowed\"}");
    return true;
}

void handleCaptivePortal() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
}

void startMdns() {
    if (mdnsStarted) return;

    if (!MDNS.begin("clipbridge")) {
        Serial.println("mDNS could not start; continuing without clipbridge.local.");
        return;
    }

    mdnsStarted = true;
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder active: http://clipbridge.local");
}

void startWebServer() {
    const char *headerKeys[] = {"Accept"};
    server.collectHeaders(headerKeys, 1);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/copy", HTTP_GET, handleCopyGet);
    server.on("/api/copy-activity", HTTP_POST, handleCopyActivity);
    server.on("/paste", HTTP_GET, handlePasteGet);
    server.on("/paste", HTTP_POST, handlePastePost);
    server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadData);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/media", HTTP_GET, handleMedia);
    server.on("/wifi", HTTP_GET, handleWifiGet);
    server.on("/wifi", HTTP_POST, handleWifiPost);
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/items", HTTP_GET, handleApiItems);
    server.on("/api/text", HTTP_POST, handleApiText);
    server.on("/api/upload", HTTP_POST, handleUploadComplete, handleUploadData);

    server.on("/generate_204", HTTP_ANY, handleCaptivePortal);
    server.on("/gen_204", HTTP_ANY, handleCaptivePortal);
    server.on("/hotspot-detect.html", HTTP_ANY, handleCaptivePortal);
    server.on("/library/test/success.html", HTTP_ANY, handleCaptivePortal);
    server.on("/ncsi.txt", HTTP_ANY, handleCaptivePortal);
    server.on("/connecttest.txt", HTTP_ANY, handleCaptivePortal);
    server.on("/redirect", HTTP_ANY, handleCaptivePortal);
    server.onNotFound([]() {
        if (handleApiDynamicRoute()) return;
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "");
    });
    server.begin();
}

// -----------------------------------------------------------------------------
// Deferred optional hardware initialization
// -----------------------------------------------------------------------------
bool parseHistoryRecord(const String &line, ClipItem &item) {
    String fields[5];
    size_t fieldIndex = 0;
    int start = 0;

    for (int i = 0; i <= static_cast<int>(line.length()) && fieldIndex < 5; ++i) {
        if (i == static_cast<int>(line.length()) || line[i] == '|') {
            fields[fieldIndex++] = line.substring(start, i);
            start = i + 1;
        }
    }
    if (fieldIndex < 5 || fields[0].length() != 1) return false;

    const char code = fields[0][0];
    if (code == 'T') item.kind = ClipKind::Text;
    else if (code == 'L') item.kind = ClipKind::Link;
    else if (code == 'I') item.kind = ClipKind::Image;
    else if (code == 'F') item.kind = ClipKind::File;
    else return false;

    item.valid = true;
    item.title = historyFieldDecode(fields[1]);
    const String payload = historyFieldDecode(fields[2]);
    item.mimeType = historyFieldDecode(fields[3]);
    item.fileSize = static_cast<uint32_t>(fields[4].toInt());

    if (item.kind == ClipKind::Text || item.kind == ClipKind::Link) {
        item.text = payload;
        if (item.text.length() == 0) return false;
    } else {
        item.filePath = payload;
        if (item.filePath.length() == 0 || !SD_MMC.exists(item.filePath.c_str())) return false;
    }
    return true;
}

size_t loadHistoryIndex() {
    if (!sdOk || !SD_MMC.exists("/history_v8.idx")) return 0;
    File file = SD_MMC.open("/history_v8.idx", FILE_READ);
    if (!file) return 0;

    size_t loaded = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        ClipItem item;
        if (parseHistoryRecord(line, item)) {
            if (insertHistoryItem(item, false)) ++loaded;
        }
    }
    file.close();
    if (loaded > 0) selectedHistoryIndex = 0;
    return loaded;
}

void restoreLatestFileFromStorage() {
    if (!sdOk || !preferences.getBool("file_saved", false)) return;

    const String name = preferences.getString("file_name", "");
    const String path = preferences.getString("file_path", "");
    const String mime = preferences.getString("file_mime", "application/octet-stream");
    const uint32_t size = preferences.getULong("file_size", 0);

    if (name.length() > 0 && path.length() > 0 && SD_MMC.exists(path.c_str())) {
        addFileItem(name, path, mime, size, false, false);
    }
}

void serviceBringup() {
    const uint32_t now = millis();
    if (now - lastBringupStepAt < 120U) return;
    lastBringupStepAt = now;

    switch (bringupStage) {
        case BringupStage::Start:
            ui.showToast("Checking optional hardware...");
            bringupStage = BringupStage::InitSd;
            break;

        case BringupStage::InitSd: {
            sdOk = storage.begin();
            if (sdOk) {
                const size_t loaded = loadHistoryIndex();
                if (loaded == 0) {
                    // One-time fallback for histories created by previous firmware versions.
                    String savedClip = storage.loadClipboard();
                    if (savedClip.length() > 0) {
                        addTextItem(savedClip, false, true);
                    }

                    String lines[50];
                    const size_t count = storage.loadRecentHistory(lines, 50);
                    for (size_t i = count; i > 0; --i) {
                        addTextItem(lines[i - 1], false, false);
                    }
                    restoreLatestFileFromStorage();
                }
                if (historyCount > 0) selectedHistoryIndex = 0;
                Serial.printf("SD initialized. History items: %u\n", static_cast<unsigned>(historyCount));
            } else {
                Serial.println("SD not mounted. Text works in RAM; file transfer is disabled.");
            }
            refreshHistoryUi();
            onHomeButton();
            bringupStage = BringupStage::InitNfcCopy;
            break;
        }

        case BringupStage::InitNfcCopy:
            copyNfcOk = nfcCopy.begin();
            if (copyNfcOk) {
                attachInterrupt(digitalPinToInterrupt(PIN_NFC_COPY_GPO), onNfcCopyWakeInterrupt, FALLING);
            }
            Serial.printf("NFC COPY: %s\n", copyNfcOk ? "OK" : "NOT FOUND");
            bringupStage = BringupStage::InitNfcPaste;
            break;

        case BringupStage::InitNfcPaste:
            pasteNfcOk = nfcPaste.begin();
            if (pasteNfcOk) {
                attachInterrupt(digitalPinToInterrupt(PIN_NFC_PASTE_GPO), onNfcPasteWakeInterrupt, FALLING);
            }
            Serial.printf("NFC PASTE: %s\n", pasteNfcOk ? "OK" : "NOT FOUND");
            bringupStage = BringupStage::ProgramTags;
            break;

        case BringupStage::ProgramTags:
            updateServiceUrls(true);
            refreshUiStatus();
            ui.showToast("Ready. COPY = to phone, PASTE = copy inside.");
            bringupStage = BringupStage::Done;
            break;

        case BringupStage::Done:
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// Setup and main loop
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println();
    Serial.println("=== ClipBridge UI v12 boot ===");

    pinMode(PIN_COPY_LED, OUTPUT);
    pinMode(PIN_PASTE_LED, OUTPUT);
    writeActionLed(PIN_COPY_LED, COPY_LED_ACTIVE_HIGH, false);
    writeActionLed(PIN_PASTE_LED, PASTE_LED_ACTIVE_HIGH, false);
    pinMode(PIN_VBAT_SENSE, INPUT);
    analogSetPinAttenuation(PIN_VBAT_SENSE, ADC_11db);

    if (!lcd.begin()) {
        Serial.println("LCD initialization failed.");
    }
    displayAwake = true;
    lastDisplayActivity = millis();

    lv_init();
    lv_tick_set_cb(lvMillis);

    const size_t lvBufferBytes =
        static_cast<size_t>(LCD_WIDTH) * LVGL_BUFFER_LINES * 2U;
    lvBuffer = static_cast<uint8_t *>(
        heap_caps_malloc(lvBufferBytes, MALLOC_CAP_8BIT)
    );
    if (lvBuffer == nullptr) {
        Serial.println("LVGL buffer allocation failed.");
        while (true) delay(1000);
    }

    lvDisplay = lv_display_create(lcd.width(), lcd.height());
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvDisplay, lvFlushCallback);
    lv_display_set_buffers(
        lvDisplay,
        lvBuffer,
        nullptr,
        lvBufferBytes,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    touchOk = touch.begin();
    lvTouch = lv_indev_create();
    lv_indev_set_type(lvTouch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvTouch, lvTouchReadCallback);
    lv_indev_set_long_press_time(lvTouch, 900);

    ui.begin(
        onHomeButton,
        onSettingsButton,
        onClearAllRequested,
        onViewAllRequested,
        onBrowserPrev,
        onBrowserNext,
        onBrowserOpen,
        onBrowserSend
    );
    ui.setWifiCallbacks(onWifiScanRequested, onWifiConnectRequested);
    ui.setHistoryCallbacks(selectHistory, openHistory, sendHistory);

    for (int i = 0; i < 8; ++i) {
        lv_timer_handler();
        delay(5);
    }

    // History starts empty and is restored from microSD during deferred bring-up.
    refreshHistoryUi();

    const uint16_t suffix = static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFFU);
    char ssidBuffer[32];
    snprintf(ssidBuffer, sizeof(ssidBuffer), "%s-%04X", WIFI_AP_BASE_NAME, suffix);
    apSsid = ssidBuffer;

    preferences.begin("clipbridge", false);

    WiFi.setHostname("clipbridge");
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    const IPAddress apIp(192, 168, 4, 1);
    const IPAddress apGateway(192, 168, 4, 1);
    const IPAddress apSubnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, apGateway, apSubnet);

    if (!WiFi.softAP(apSsid.c_str(), WIFI_AP_PASSWORD)) {
        Serial.println("Wi-Fi AP failed.");
    }

    updateServiceUrls(false);
    startMdns();
    dnsServer.start(53, "*", WiFi.softAPIP());
    startWebServer();
    refreshUiStatus();

    const bool hasSavedWifi = preferences.getBool("wifi_saved", false);
    const String savedSsid = preferences.getString("ssid", "");
    const String savedPass = preferences.getString("pass", "");
    if (hasSavedWifi && savedSsid.length() > 0) {
        ui.setWifiStatus(String("Reconnecting to saved Wi-Fi\n") + savedSsid);
        beginStationConnection(savedSsid, savedPass, false);
    }

    Serial.printf("Touch: %s\n", touchOk ? "OK" : "NOT FOUND");
    Serial.printf("Fallback AP: %s\n", apSsid.c_str());
    Serial.printf("COPY URL: %s\n", copyUrl.c_str());
    Serial.printf("PASTE URL: %s\n", pasteUrl.c_str());
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    lv_timer_handler();
    serviceActionLeds();
    serviceBringup();
    serviceWifiScan();
    serviceStationConnection();
    serviceDisplayPower();

    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastBatteryUpdate) >= 1500U) {
        lastBatteryUpdate = now;
        const float measured = readBatteryVoltage();
        if (measured >= VBAT_VALID_MIN_V && measured <= VBAT_VALID_MAX_V) {
            if (filteredBatteryVoltage <= 0.1f) filteredBatteryVoltage = measured;
            else filteredBatteryVoltage = filteredBatteryVoltage * 0.82f + measured * 0.18f;
            ui.setBattery(filteredBatteryVoltage, batteryPercent(filteredBatteryVoltage));
        } else {
            ui.setBattery(0.0f, BATTERY_FALLBACK_PERCENT);
        }
    }

    delay(3);
}

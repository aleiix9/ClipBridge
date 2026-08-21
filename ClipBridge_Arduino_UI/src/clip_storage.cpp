#include "clip_storage.h"

#include <FS.h>
#include <SD_MMC.h>
#include "clipbridge_config.h"

bool ClipStorage::begin() {
    if (!SD_MMC.setPins(
            PIN_SD_CLK,
            PIN_SD_CMD,
            PIN_SD_D0,
            PIN_SD_D1,
            PIN_SD_D2,
            PIN_SD_D3
        )) {
        mounted_ = false;
        return false;
    }

    mounted_ = SD_MMC.begin("/sdcard", false);
    return mounted_;
}

String ClipStorage::loadClipboard() {
    if (!mounted_ || !SD_MMC.exists("/clip.txt")) {
        return String();
    }

    File file = SD_MMC.open("/clip.txt", FILE_READ);
    if (!file) {
        return String();
    }

    String text;
    text.reserve(CLIPBOARD_MAX_CHARS);

    while (file.available() && text.length() < CLIPBOARD_MAX_CHARS) {
        text += static_cast<char>(file.read());
    }

    file.close();
    return text;
}

bool ClipStorage::saveClipboard(const String &text) {
    if (!mounted_) {
        return false;
    }

    File file = SD_MMC.open("/clip.txt", FILE_WRITE);
    if (!file) {
        return false;
    }

    file.seek(0);
    file.print(text);
    file.close();
    return true;
}

bool ClipStorage::appendHistory(const String &text) {
    if (!mounted_) {
        return false;
    }

    File file = SD_MMC.open("/history.txt", FILE_APPEND);
    if (!file) {
        return false;
    }

    String flattened = text;
    flattened.replace("\r", " ");
    flattened.replace("\n", " ");
    if (flattened.length() > 180) {
        flattened = flattened.substring(0, 180);
        flattened += "...";
    }

    file.println(flattened);
    file.close();
    return true;
}

size_t ClipStorage::loadRecentHistory(String *lines, size_t max_lines) {
    if (!mounted_ || lines == nullptr || max_lines == 0 || !SD_MMC.exists("/history.txt")) {
        return 0;
    }

    File file = SD_MMC.open("/history.txt", FILE_READ);
    if (!file) {
        return 0;
    }

    size_t total = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) {
            continue;
        }

        if (total < max_lines) {
            lines[total++] = line;
        } else {
            for (size_t i = 1; i < max_lines; ++i) {
                lines[i - 1] = lines[i];
            }
            lines[max_lines - 1] = line;
        }
    }

    file.close();

    for (size_t i = 0; i < total / 2; ++i) {
        String tmp = lines[i];
        lines[i] = lines[total - 1 - i];
        lines[total - 1 - i] = tmp;
    }

    return total;
}

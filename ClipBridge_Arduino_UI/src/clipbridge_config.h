#pragma once

#include <Arduino.h>

/*
 * ClipBridge hardware configuration
 * ESP32-S3 pinout taken from the EasyEDA schematic shared in the project.
 *
 * IMPORTANT:
 * - GPIO0 is reserved for BOOT/download mode.
 * - EN is the reset input.
 * - GPIO19/20 are native USB D-/D+.
 * - The two ST25DV devices use software I2C because the design has three
 *   physically separate I2C buses (touch, NFC COPY and NFC PASTE), while the
 *   ESP32-S3 has only two hardware I2C controllers.
 */

// -------------------------
// LCD (ST7789, SPI)
// -------------------------
static constexpr int PIN_LCD_BL   = 7;
static constexpr int PIN_LCD_RST  = 8;
static constexpr int PIN_LCD_DC   = 9;
static constexpr int PIN_LCD_CS   = 10;
static constexpr int PIN_LCD_MOSI = 11;
static constexpr int PIN_LCD_SCK  = 12;

static constexpr uint16_t LCD_WIDTH  = 240;
static constexpr uint16_t LCD_HEIGHT = 320;
static constexpr uint32_t LCD_SPI_HZ = 40000000UL;

// 0 is expected for the physical portrait orientation.
// If the image is upside down, try 2.
// If it is sideways, try 1 or 3 and also adapt the LVGL resolution.
static constexpr uint8_t LCD_ROTATION = 2;

// Set true if red and blue are exchanged.
static constexpr bool LCD_USE_BGR = false;

// -------------------------
// Touch (CST328, hardware I2C)
// -------------------------
static constexpr int PIN_TOUCH_SCL = 4;
static constexpr int PIN_TOUCH_SDA = 5;
static constexpr int PIN_TOUCH_INT = 6;
static constexpr int PIN_TOUCH_RST = 15;

static constexpr uint8_t TOUCH_I2C_ADDR = 0x1A;
static constexpr uint32_t TOUCH_I2C_HZ = 400000UL;

// Touch orientation correction. Change only if touch coordinates do not match.
static constexpr bool TOUCH_SWAP_XY  = false;
static constexpr bool TOUCH_MIRROR_X = false;
static constexpr bool TOUCH_MIRROR_Y = false;

// Keeps one physical tap as one continuous LVGL press even if the
// CST328 briefly drops packets while the finger is still on the panel.
static constexpr uint32_t TOUCH_RELEASE_DEBOUNCE_MS = 75U;

// -------------------------
// NFC COPY (ST25DV04KC, software I2C)
// -------------------------
// Physical upper COPY zone. Verified on the assembled PCB: this zone is on
// the GPIO40/39/38 ST25DV bus.
static constexpr int PIN_NFC_COPY_GPO = 40;
static constexpr int PIN_NFC_COPY_SDA = 39;
static constexpr int PIN_NFC_COPY_SCL = 38;

// -------------------------
// NFC PASTE (ST25DV04KC, software I2C)
// -------------------------
// Physical lower PASTE zone. Verified on the assembled PCB: this zone is on
// the GPIO16/17/18 ST25DV bus.
static constexpr int PIN_NFC_PASTE_GPO = 16;
static constexpr int PIN_NFC_PASTE_SDA = 17;
static constexpr int PIN_NFC_PASTE_SCL = 18;

static constexpr uint8_t ST25DV_USER_ADDR = 0x53;
static constexpr uint8_t ST25DV_SYSTEM_ADDR = 0x57;
static constexpr uint32_t SOFT_I2C_HALF_PERIOD_US = 5; // about 100 kHz

// Reprograms only when content differs, so this can remain true.
static constexpr bool NFC_PROGRAM_URLS_ON_BOOT = true;

// -------------------------
// microSD (4-bit SD_MMC)
// -------------------------
static constexpr int PIN_SD_D0  = 13;
static constexpr int PIN_SD_CMD = 14;
static constexpr int PIN_SD_CLK = 21;
static constexpr int PIN_SD_D3  = 47;
static constexpr int PIN_SD_D2  = 42;
static constexpr int PIN_SD_D1  = 41;

// -------------------------
// Battery / action LEDs
// -------------------------
static constexpr int PIN_VBAT_SENSE = 1;

// Physical feedback LEDs beside the two NFC zones.
// Current assembled unit has the two action LED nets crossed relative to the
// enclosure labels, so COPY feedback is driven from GPIO48 and PASTE/COPY
// INSIDE feedback is driven from GPIO2.
// Both are wired GPIO -> resistor -> LED -> GND, therefore active HIGH.
static constexpr int PIN_COPY_LED = 48;
static constexpr int PIN_PASTE_LED = 2;
static constexpr bool COPY_LED_ACTIVE_HIGH = true;
static constexpr bool PASTE_LED_ACTIVE_HIGH = true;
static constexpr uint32_t ACTION_LED_MIN_MS = 3000U;

// Change this to the exact resistor-divider ratio:
// ratio = (Rtop + Rbottom) / Rbottom
// Example 100k/100k => 2.0
static constexpr float VBAT_DIVIDER_RATIO = 2.0f;
static constexpr float VBAT_EMPTY_V = 3.35f;
static constexpr float VBAT_FULL_V  = 4.18f;

// Battery UI fallback. If the ADC reading is implausible (for example while
// debugging a board without the divider populated), the UI still shows a
// stable value instead of "--%".
static constexpr uint8_t BATTERY_FALLBACK_PERCENT = 95;
static constexpr float VBAT_VALID_MIN_V = 3.0f;
static constexpr float VBAT_VALID_MAX_V = 4.35f;

// The device has no hard power button. Keep the electronics alive and only
// switch off the LCD backlight after inactivity. Touch or either NFC GPO wakes
// the display immediately.
static constexpr uint32_t DISPLAY_BACKLIGHT_TIMEOUT_MS = 30000U;

// -------------------------
// Wi-Fi access point / web clipboard
// -------------------------
static constexpr char WIFI_AP_BASE_NAME[] = "ClipBridge";
static constexpr char WIFI_AP_PASSWORD[]  = "clipbridge"; // >= 8 chars
static constexpr char COPY_PATH[]  = "/copy";
static constexpr char PASTE_PATH[] = "/paste";

// -------------------------
// LVGL
// -------------------------
static constexpr uint16_t LVGL_BUFFER_LINES = 24;

// Clipboard limits keep RAM and SD usage predictable.
static constexpr size_t CLIPBOARD_MAX_CHARS = 1200;

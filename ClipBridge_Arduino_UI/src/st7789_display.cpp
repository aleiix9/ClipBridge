#include "st7789_display.h"

namespace {
constexpr uint8_t CMD_SWRESET = 0x01;
constexpr uint8_t CMD_SLPOUT  = 0x11;
constexpr uint8_t CMD_NORON   = 0x13;
constexpr uint8_t CMD_INVOFF  = 0x20;
constexpr uint8_t CMD_INVON   = 0x21;
constexpr uint8_t CMD_DISPON  = 0x29;
constexpr uint8_t CMD_CASET   = 0x2A;
constexpr uint8_t CMD_RASET   = 0x2B;
constexpr uint8_t CMD_RAMWR   = 0x2C;
constexpr uint8_t CMD_MADCTL  = 0x36;
constexpr uint8_t CMD_COLMOD  = 0x3A;

constexpr uint8_t MADCTL_MY  = 0x80;
constexpr uint8_t MADCTL_MX  = 0x40;
constexpr uint8_t MADCTL_MV  = 0x20;
constexpr uint8_t MADCTL_BGR = 0x08;
}

bool ST7789Display::begin() {
    pinMode(PIN_LCD_CS, OUTPUT);
    pinMode(PIN_LCD_DC, OUTPUT);
    pinMode(PIN_LCD_RST, OUTPUT);
    pinMode(PIN_LCD_BL, OUTPUT);

    digitalWrite(PIN_LCD_CS, HIGH);
    digitalWrite(PIN_LCD_DC, HIGH);
    digitalWrite(PIN_LCD_BL, LOW);

    spi_->begin(PIN_LCD_SCK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
    hardwareReset();

    writeCommand(CMD_SWRESET);
    delay(150);

    writeCommand(CMD_SLPOUT);
    delay(120);

    const uint8_t color_mode = 0x55; // RGB565
    writeCommandData(CMD_COLMOD, &color_mode, 1);
    delay(10);

    // Standard ST7789 power/timing configuration used by many 240x320 modules.
    const uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    writeCommandData(0xB2, porch, sizeof(porch));

    const uint8_t gate = 0x35;
    writeCommandData(0xB7, &gate, 1);

    const uint8_t vcom = 0x19;
    writeCommandData(0xBB, &vcom, 1);

    const uint8_t lcm = 0x2C;
    writeCommandData(0xC0, &lcm, 1);

    const uint8_t vdv_vrh_en = 0x01;
    writeCommandData(0xC2, &vdv_vrh_en, 1);

    const uint8_t vrh = 0x12;
    writeCommandData(0xC3, &vrh, 1);

    const uint8_t vdv = 0x20;
    writeCommandData(0xC4, &vdv, 1);

    const uint8_t frame = 0x0F;
    writeCommandData(0xC6, &frame, 1);

    const uint8_t power[] = {0xA4, 0xA1};
    writeCommandData(0xD0, power, sizeof(power));

    const uint8_t gamma_pos[] = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23
    };
    writeCommandData(0xE0, gamma_pos, sizeof(gamma_pos));

    const uint8_t gamma_neg[] = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23
    };
    writeCommandData(0xE1, gamma_neg, sizeof(gamma_neg));

    setRotation(LCD_ROTATION);

    writeCommand(CMD_INVON);
    delay(10);
    writeCommand(CMD_NORON);
    delay(10);
    writeCommand(CMD_DISPON);
    delay(120);

    setBacklight(true);
    return true;
}

void ST7789Display::setBacklight(bool on) {
    backlight_on_ = on;
    digitalWrite(PIN_LCD_BL, on ? HIGH : LOW);
}

void ST7789Display::hardwareReset() {
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(20);
    digitalWrite(PIN_LCD_RST, LOW);
    delay(20);
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(120);
}

void ST7789Display::setRotation(uint8_t rotation) {
    rotation &= 3U;
    uint8_t madctl = 0;

    switch (rotation) {
        case 0:
            width_ = LCD_WIDTH;
            height_ = LCD_HEIGHT;
            madctl = MADCTL_MX | MADCTL_MY;
            break;
        case 1:
            width_ = LCD_HEIGHT;
            height_ = LCD_WIDTH;
            madctl = MADCTL_MY | MADCTL_MV;
            break;
        case 2:
            width_ = LCD_WIDTH;
            height_ = LCD_HEIGHT;
            madctl = 0;
            break;
        case 3:
            width_ = LCD_HEIGHT;
            height_ = LCD_WIDTH;
            madctl = MADCTL_MX | MADCTL_MV;
            break;
    }

    if (LCD_USE_BGR) {
        madctl |= MADCTL_BGR;
    }
    writeCommandData(CMD_MADCTL, &madctl, 1);
}

void ST7789Display::writeCommand(uint8_t cmd) {
    spi_->beginTransaction(SPISettings(LCD_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LCD_CS, LOW);
    sendCommandInTransaction(cmd);
    digitalWrite(PIN_LCD_CS, HIGH);
    spi_->endTransaction();
}

void ST7789Display::writeCommandData(uint8_t cmd, const uint8_t *data, size_t len) {
    spi_->beginTransaction(SPISettings(LCD_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LCD_CS, LOW);
    sendCommandInTransaction(cmd);
    if (len > 0 && data != nullptr) {
        sendDataInTransaction(data, len);
    }
    digitalWrite(PIN_LCD_CS, HIGH);
    spi_->endTransaction();
}

void ST7789Display::sendCommandInTransaction(uint8_t cmd) {
    digitalWrite(PIN_LCD_DC, LOW);
    spi_->transfer(cmd);
}

void ST7789Display::sendDataInTransaction(const uint8_t *data, size_t len) {
    digitalWrite(PIN_LCD_DC, HIGH);
    spi_->writeBytes(data, len);
}

void ST7789Display::setAddressWindowInTransaction(
    uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1
) {
    const uint8_t col[] = {
        static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0),
        static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1)
    };
    const uint8_t row[] = {
        static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0),
        static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1)
    };

    sendCommandInTransaction(CMD_CASET);
    sendDataInTransaction(col, sizeof(col));
    sendCommandInTransaction(CMD_RASET);
    sendDataInTransaction(row, sizeof(row));
    sendCommandInTransaction(CMD_RAMWR);
    digitalWrite(PIN_LCD_DC, HIGH);
}

void ST7789Display::flush(const lv_area_t *area, const uint8_t *px_map) {
    if (area == nullptr || px_map == nullptr) {
        return;
    }

    const uint16_t x0 = static_cast<uint16_t>(area->x1);
    const uint16_t y0 = static_cast<uint16_t>(area->y1);
    const uint16_t x1 = static_cast<uint16_t>(area->x2);
    const uint16_t y1 = static_cast<uint16_t>(area->y2);
    const uint16_t w = x1 - x0 + 1U;
    const uint16_t h = y1 - y0 + 1U;

    const uint16_t *src = reinterpret_cast<const uint16_t *>(px_map);

    spi_->beginTransaction(SPISettings(LCD_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_LCD_CS, LOW);
    setAddressWindowInTransaction(x0, y0, x1, y1);

    for (uint16_t row = 0; row < h; ++row) {
        const uint16_t *row_src = src + static_cast<size_t>(row) * w;
        for (uint16_t col = 0; col < w; ++col) {
            const uint16_t color = row_src[col];
            // ST7789 expects RGB565 MSB first.
            line_buffer_[col * 2U]     = static_cast<uint8_t>(color >> 8);
            line_buffer_[col * 2U + 1] = static_cast<uint8_t>(color & 0xFF);
        }
        spi_->writeBytes(line_buffer_, static_cast<size_t>(w) * 2U);
    }

    digitalWrite(PIN_LCD_CS, HIGH);
    spi_->endTransaction();
}

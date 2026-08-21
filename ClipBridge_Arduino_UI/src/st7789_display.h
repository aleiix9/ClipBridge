#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "clipbridge_config.h"
#include "lvgl/lvgl.h"

class ST7789Display {
public:
    bool begin();
    void flush(const lv_area_t *area, const uint8_t *px_map);
    void setBacklight(bool on);
    bool backlightOn() const { return backlight_on_; }
    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }

private:
    void hardwareReset();
    void setRotation(uint8_t rotation);
    void writeCommand(uint8_t cmd);
    void writeCommandData(uint8_t cmd, const uint8_t *data, size_t len);
    void sendCommandInTransaction(uint8_t cmd);
    void sendDataInTransaction(const uint8_t *data, size_t len);
    void setAddressWindowInTransaction(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    SPIClass *spi_ = &SPI;
    uint16_t width_ = LCD_WIDTH;
    uint16_t height_ = LCD_HEIGHT;
    uint8_t line_buffer_[(LCD_WIDTH > LCD_HEIGHT ? LCD_WIDTH : LCD_HEIGHT) * 2];
    bool backlight_on_ = false;
};

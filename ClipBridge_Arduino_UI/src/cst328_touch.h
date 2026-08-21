#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "clipbridge_config.h"

class CST328Touch {
public:
    bool begin(TwoWire &wire = Wire);
    bool readPoint(uint16_t &x, uint16_t &y);
    bool present() const { return present_; }

private:
    bool readRegister(uint16_t reg, uint8_t *data, size_t len);
    bool writeRegister(uint16_t reg, const uint8_t *data, size_t len);
    void reset();
    void transform(uint16_t &x, uint16_t &y);

    TwoWire *wire_ = nullptr;
    bool present_ = false;
};

#include "cst328_touch.h"

namespace {
constexpr uint16_t REG_TOUCH_POINTS = 0xD005;
constexpr uint16_t REG_TOUCH_DATA   = 0xD000;
}

bool CST328Touch::begin(TwoWire &wire) {
    wire_ = &wire;

    pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
    pinMode(PIN_TOUCH_RST, OUTPUT);

    wire_->begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, TOUCH_I2C_HZ);
    reset();

    wire_->beginTransmission(TOUCH_I2C_ADDR);
    present_ = (wire_->endTransmission() == 0);

    // Clear any stale touch packet.
    if (present_) {
        const uint8_t clear = 0;
        writeRegister(REG_TOUCH_POINTS, &clear, 1);
    }

    return present_;
}

void CST328Touch::reset() {
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(50);
    digitalWrite(PIN_TOUCH_RST, LOW);
    delay(5);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(80);
}

bool CST328Touch::readRegister(uint16_t reg, uint8_t *data, size_t len) {
    if (wire_ == nullptr || data == nullptr || len == 0) {
        return false;
    }

    wire_->beginTransmission(TOUCH_I2C_ADDR);
    wire_->write(static_cast<uint8_t>(reg >> 8));
    wire_->write(static_cast<uint8_t>(reg & 0xFF));

    if (wire_->endTransmission(true) != 0) {
        return false;
    }

    const size_t received = wire_->requestFrom(
        static_cast<int>(TOUCH_I2C_ADDR),
        static_cast<int>(len),
        static_cast<int>(true)
    );

    if (received != len) {
        while (wire_->available()) {
            (void)wire_->read();
        }
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(wire_->read());
    }
    return true;
}

bool CST328Touch::writeRegister(uint16_t reg, const uint8_t *data, size_t len) {
    if (wire_ == nullptr) {
        return false;
    }

    wire_->beginTransmission(TOUCH_I2C_ADDR);
    wire_->write(static_cast<uint8_t>(reg >> 8));
    wire_->write(static_cast<uint8_t>(reg & 0xFF));

    for (size_t i = 0; i < len; ++i) {
        wire_->write(data[i]);
    }

    return wire_->endTransmission(true) == 0;
}

void CST328Touch::transform(uint16_t &x, uint16_t &y) {
    if (TOUCH_SWAP_XY) {
        const uint16_t tmp = x;
        x = y;
        y = tmp;
    }

    const uint16_t max_x = TOUCH_SWAP_XY ? LCD_HEIGHT : LCD_WIDTH;
    const uint16_t max_y = TOUCH_SWAP_XY ? LCD_WIDTH : LCD_HEIGHT;

    if (TOUCH_MIRROR_X && x < max_x) {
        x = max_x - 1U - x;
    }
    if (TOUCH_MIRROR_Y && y < max_y) {
        y = max_y - 1U - y;
    }

    if (x >= max_x) {
        x = max_x - 1U;
    }
    if (y >= max_y) {
        y = max_y - 1U;
    }
}

bool CST328Touch::readPoint(uint16_t &x, uint16_t &y) {
    if (!present_) {
        return false;
    }

    uint8_t count = 0;
    if (!readRegister(REG_TOUCH_POINTS, &count, 1)) {
        return false;
    }

    const uint8_t point_count = count & 0x0F;
    if (point_count == 0 || point_count > 5) {
        const uint8_t clear = 0;
        (void)writeRegister(REG_TOUCH_POINTS, &clear, 1);
        return false;
    }

    uint8_t data[27] = {0};
    if (!readRegister(REG_TOUCH_DATA, data, sizeof(data))) {
        return false;
    }

    // Parsing follows the CST328 packet format used by Waveshare's demo.
    x = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[1]) << 4) |
        ((data[3] & 0xF0) >> 4)
    );
    y = static_cast<uint16_t>(
        (static_cast<uint16_t>(data[2]) << 4) |
        (data[3] & 0x0F)
    );

    const uint8_t clear = 0;
    (void)writeRegister(REG_TOUCH_POINTS, &clear, 1);

    transform(x, y);
    return true;
}

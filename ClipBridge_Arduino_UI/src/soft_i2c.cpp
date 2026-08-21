#include "soft_i2c.h"

SoftI2C::SoftI2C(int sda, int scl, uint32_t half_period_us)
    : sda_(sda), scl_(scl), half_period_us_(half_period_us) {}

void SoftI2C::begin() {
    sdaRelease();
    sclRelease();
    delayHalf();
}

void SoftI2C::sdaLow() {
    pinMode(sda_, OUTPUT);
    digitalWrite(sda_, LOW);
}

void SoftI2C::sdaRelease() {
    pinMode(sda_, INPUT_PULLUP);
}

void SoftI2C::sclLow() {
    pinMode(scl_, OUTPUT);
    digitalWrite(scl_, LOW);
}

void SoftI2C::sclRelease() {
    pinMode(scl_, INPUT_PULLUP);
}

bool SoftI2C::waitSclHigh(uint32_t timeout_us) {
    sclRelease();
    const uint32_t start = micros();
    while (digitalRead(scl_) == LOW) {
        if (static_cast<uint32_t>(micros() - start) > timeout_us) {
            return false;
        }
    }
    return true;
}

void SoftI2C::delayHalf() const {
    delayMicroseconds(half_period_us_);
}

void SoftI2C::startCondition() {
    sdaRelease();
    waitSclHigh();
    delayHalf();
    sdaLow();
    delayHalf();
    sclLow();
}

void SoftI2C::stopCondition() {
    sdaLow();
    delayHalf();
    waitSclHigh();
    delayHalf();
    sdaRelease();
    delayHalf();
}

bool SoftI2C::writeByte(uint8_t value) {
    for (int bit = 7; bit >= 0; --bit) {
        if (value & (1U << bit)) {
            sdaRelease();
        } else {
            sdaLow();
        }
        delayHalf();
        if (!waitSclHigh()) {
            sclLow();
            return false;
        }
        delayHalf();
        sclLow();
    }

    // ACK bit.
    sdaRelease();
    delayHalf();
    if (!waitSclHigh()) {
        sclLow();
        return false;
    }
    const bool ack = (digitalRead(sda_) == LOW);
    delayHalf();
    sclLow();
    return ack;
}

uint8_t SoftI2C::readByte(bool ack) {
    uint8_t value = 0;
    sdaRelease();

    for (int bit = 7; bit >= 0; --bit) {
        delayHalf();
        waitSclHigh();
        if (digitalRead(sda_) == HIGH) {
            value |= static_cast<uint8_t>(1U << bit);
        }
        delayHalf();
        sclLow();
    }

    if (ack) {
        sdaLow();
    } else {
        sdaRelease();
    }

    delayHalf();
    waitSclHigh();
    delayHalf();
    sclLow();
    sdaRelease();

    return value;
}

bool SoftI2C::probe(uint8_t address) {
    noInterrupts();
    startCondition();
    const bool ok = writeByte(static_cast<uint8_t>(address << 1));
    stopCondition();
    interrupts();
    return ok;
}

bool SoftI2C::readMemory(
    uint8_t address,
    uint16_t memory_address,
    uint8_t *data,
    size_t length
) {
    if (data == nullptr || length == 0) {
        return false;
    }

    noInterrupts();

    startCondition();
    if (!writeByte(static_cast<uint8_t>(address << 1))) {
        stopCondition();
        interrupts();
        return false;
    }
    if (!writeByte(static_cast<uint8_t>(memory_address >> 8)) ||
        !writeByte(static_cast<uint8_t>(memory_address & 0xFF))) {
        stopCondition();
        interrupts();
        return false;
    }

    startCondition();
    if (!writeByte(static_cast<uint8_t>((address << 1) | 0x01))) {
        stopCondition();
        interrupts();
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        data[i] = readByte(i + 1U < length);
    }

    stopCondition();
    interrupts();
    return true;
}

bool SoftI2C::waitReady(uint8_t address, uint32_t timeout_ms) {
    const uint32_t start = millis();
    while (static_cast<uint32_t>(millis() - start) < timeout_ms) {
        if (probe(address)) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool SoftI2C::writeMemory(
    uint8_t address,
    uint16_t memory_address,
    const uint8_t *data,
    size_t length,
    size_t page_size
) {
    if (data == nullptr || length == 0 || page_size == 0) {
        return false;
    }

    size_t offset = 0;

    while (offset < length) {
        const uint16_t current_address =
            static_cast<uint16_t>(memory_address + offset);
        const size_t room_in_page =
            page_size - (current_address % page_size);
        const size_t chunk =
            min(room_in_page, length - offset);

        noInterrupts();
        startCondition();

        bool ok = writeByte(static_cast<uint8_t>(address << 1));
        ok = ok && writeByte(static_cast<uint8_t>(current_address >> 8));
        ok = ok && writeByte(static_cast<uint8_t>(current_address & 0xFF));

        for (size_t i = 0; ok && i < chunk; ++i) {
            ok = writeByte(data[offset + i]);
        }

        stopCondition();
        interrupts();

        if (!ok || !waitReady(address, 20)) {
            return false;
        }

        offset += chunk;
    }

    return true;
}

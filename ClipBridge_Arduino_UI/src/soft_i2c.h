#pragma once

#include <Arduino.h>

class SoftI2C {
public:
    SoftI2C(int sda, int scl, uint32_t half_period_us);

    void begin();
    bool probe(uint8_t address);

    bool readMemory(
        uint8_t address,
        uint16_t memory_address,
        uint8_t *data,
        size_t length
    );

    bool writeMemory(
        uint8_t address,
        uint16_t memory_address,
        const uint8_t *data,
        size_t length,
        size_t page_size = 16
    );

private:
    void sdaLow();
    void sdaRelease();
    void sclLow();
    void sclRelease();
    bool waitSclHigh(uint32_t timeout_us = 1000);

    void delayHalf() const;
    void startCondition();
    void stopCondition();
    bool writeByte(uint8_t value);
    uint8_t readByte(bool ack);
    bool waitReady(uint8_t address, uint32_t timeout_ms);

    int sda_;
    int scl_;
    uint32_t half_period_us_;
};

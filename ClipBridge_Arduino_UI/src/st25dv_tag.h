#pragma once

#include <Arduino.h>
#include "clipbridge_config.h"
#include "soft_i2c.h"

class ST25DVTag {
public:
    ST25DVTag(
        const char *name,
        int sda_pin,
        int scl_pin,
        int gpo_pin
    );

    bool begin();
    bool present() const { return present_; }
    const char *name() const { return name_; }

    // Writes a Type 5 NDEF URI only when the stored bytes differ.
    bool ensureUri(const String &url);

    // GPO is open-drain on the 8-pin device. ST25DVxxKC powers up with
    // FIELD_CHANGE enabled, so an RF field transition can be used as a
    // short wake pulse by the host.
    bool gpoActive() const;

private:
    size_t buildUriImage(const String &url, uint8_t *output, size_t max_len);
    bool compareMemory(const uint8_t *expected, size_t length);

    const char *name_;
    int gpo_pin_;
    SoftI2C bus_;
    bool present_ = false;
};

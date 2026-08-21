#include "st25dv_tag.h"

#include <cstring>

ST25DVTag::ST25DVTag(
    const char *name,
    int sda_pin,
    int scl_pin,
    int gpo_pin
)
    : name_(name),
      gpo_pin_(gpo_pin),
      bus_(sda_pin, scl_pin, SOFT_I2C_HALF_PERIOD_US) {}

bool ST25DVTag::begin() {
    pinMode(gpo_pin_, INPUT_PULLUP);
    bus_.begin();
    present_ = bus_.probe(ST25DV_USER_ADDR);
    return present_;
}

bool ST25DVTag::gpoActive() const {
    return digitalRead(gpo_pin_) == LOW;
}

size_t ST25DVTag::buildUriImage(
    const String &url,
    uint8_t *output,
    size_t max_len
) {
    if (output == nullptr || max_len < 16) {
        return 0;
    }

    String remainder = url;
    uint8_t uri_prefix = 0x00;

    if (remainder.startsWith("http://www.")) {
        uri_prefix = 0x01;
        remainder.remove(0, 11);
    } else if (remainder.startsWith("https://www.")) {
        uri_prefix = 0x02;
        remainder.remove(0, 12);
    } else if (remainder.startsWith("http://")) {
        uri_prefix = 0x03;
        remainder.remove(0, 7);
    } else if (remainder.startsWith("https://")) {
        uri_prefix = 0x04;
        remainder.remove(0, 8);
    }

    const size_t payload_len = 1U + remainder.length();
    const size_t record_len = 5U + remainder.length();
    const size_t total_len = 4U + 2U + record_len + 1U;

    if (payload_len > 255U || record_len > 254U || total_len > max_len) {
        return 0;
    }

    size_t i = 0;

    // NFC Forum Type 5 Capability Container for 512-byte ST25DV04KC.
    output[i++] = 0xE1;
    output[i++] = 0x40;
    output[i++] = 0x40; // 64 * 8 bytes = 512 bytes
    output[i++] = 0x05;

    // NDEF TLV.
    output[i++] = 0x03;
    output[i++] = static_cast<uint8_t>(record_len);

    // Short Well-Known URI record.
    output[i++] = 0xD1; // MB | ME | SR | TNF well-known
    output[i++] = 0x01; // type length
    output[i++] = static_cast<uint8_t>(payload_len);
    output[i++] = 0x55; // 'U'
    output[i++] = uri_prefix;

    for (size_t c = 0; c < remainder.length(); ++c) {
        output[i++] = static_cast<uint8_t>(remainder[c]);
    }

    output[i++] = 0xFE; // Terminator TLV
    return i;
}

bool ST25DVTag::compareMemory(
    const uint8_t *expected,
    size_t length
) {
    if (expected == nullptr || length == 0 || length > 192) {
        return false;
    }

    uint8_t current[192] = {0};
    if (!bus_.readMemory(ST25DV_USER_ADDR, 0x0000, current, length)) {
        return false;
    }

    return std::memcmp(current, expected, length) == 0;
}

bool ST25DVTag::ensureUri(const String &url) {
    if (!present_) {
        return false;
    }

    uint8_t image[192] = {0};
    const size_t image_len = buildUriImage(url, image, sizeof(image));
    if (image_len == 0) {
        return false;
    }

    if (compareMemory(image, image_len)) {
        return true;
    }

    return bus_.writeMemory(
        ST25DV_USER_ADDR,
        0x0000,
        image,
        image_len,
        16
    );
}

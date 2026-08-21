#pragma once

#include <Arduino.h>

class ClipStorage {
public:
    bool begin();
    bool available() const { return mounted_; }

    String loadClipboard();
    bool saveClipboard(const String &text);
    bool appendHistory(const String &text);
    size_t loadRecentHistory(String *lines, size_t max_lines);

private:
    bool mounted_ = false;
};

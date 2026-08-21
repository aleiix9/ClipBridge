# ClipBridge Firmware Status — UI v11

v11 keeps the working v8 transfer/storage architecture and concentrates the changes on display usability and always-on operation.

Implemented in this revision:

- simplified fixed 240 x 320 Home UI with `ClipBridge / by Aleix Ferrer. No app needed.`, `COPY TO PHONE`, and `COPY INSIDE` guidance panels;
- rebuilt CURRENT ITEM card with fixed title/type/status bounds so they cannot overlap;
- simplified four-row History UI with row-tap selection and automatic return to Home;
- compacted filter rows;
- top-bar battery percentage with measured value when plausible and 95% fallback otherwise;
- LCD backlight timeout after 30 seconds of inactivity;
- wake by touchscreen without accidental first-touch activation;
- wake request latched from either ST25DV GPO using GPIO interrupts;
- corrected physical NFC bus assignment so the upper COPY and lower PASTE zones match their user-facing functions;
- existing Wi-Fi, web clipboard, NFC URL programming, microSD file/history support and settings flow preserved.

Validation performed in this package:

- source tree and version references checked;
- `clipbridge_ui.cpp` passed `g++ -fsyntax-only` against the included LVGL headers with a local Arduino API stub after the v11 UI changes;
- the main `.ino` modifications were checked for balanced delimiters and expected ESP32/Arduino symbols;
- UI geometry reviewed against the fixed 240 x 320 coordinate budget so Home/History content stays above the 44 px bottom navigation.

Final validation still requires Arduino IDE compilation with the exact installed ESP32 core and a physical-board smoke test for touch coordinates, VBAT divider calibration and ST25DV GPO wiring/pull-up behavior.

# ClipBridge Arduino UI v11 — start here

Open `ClipBridge_Arduino_UI_v11.ino` with Arduino IDE 2.x. Keep the whole folder together because LVGL and all custom drivers are included under `src/`.

## Arduino configuration

- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- USB Mode: `Hardware CDC and JTAG`
- PSRAM: `Disabled`
- Partition Scheme: `Huge APP (3MB No OTA)`
- Serial Monitor: `115200 baud`

## Normal use

1. Home shows **COPY TO PHONE** and **COPY INSIDE**, plus `ClipBridge — by Aleix Ferrer. No app needed.`.
2. COPY INSIDE adds text, images or files from the phone to ClipBridge through the physical PASTE zone.
3. Open History and tap an item to make it the CURRENT ITEM for COPY TO PHONE; the screen returns to Home.
4. Tap the phone on the physical COPY zone to retrieve the selected item onto that phone.
5. After 30 seconds without touch/NFC activity, only the LCD backlight turns off. Touch or NFC wakes it again.

## Wi-Fi

Settings opens network scanning and password entry. Successful credentials are stored in ESP32 NVS and reused after power cycling.

Fallback AP:

- SSID: `ClipBridge-XXXX`
- Password: `clipbridge`
- IP: `192.168.4.1`

The phone must be on the same external Wi-Fi as ClipBridge, or connected to the fallback ClipBridge AP.

## Expected serial messages

```text
=== ClipBridge UI v11 boot ===
Touch: OK
Fallback AP: ClipBridge-XXXX
NFC COPY: OK
NFC PASTE: OK
COPY NFC URL: http://<device-ip>/copy
PASTE NFC URL: http://<device-ip>/paste
```

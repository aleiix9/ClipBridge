# ClipBridge UI v11

Open `ClipBridge_Arduino_UI_v11.ino` in Arduino IDE 2.x and keep the complete folder together.

## Arduino IDE settings

- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- USB Mode: `Hardware CDC and JTAG`
- PSRAM: `Disabled`
- Partition Scheme: `Huge APP (3 MB No OTA)`
- Serial Monitor: `115200 baud`

## What changed in v11

### Home UI: deliberately simple

The 240 x 320 Home screen is now built around the two physical actions instead of a dense status dashboard:

- **ClipBridge** branding with `by Aleix Ferrer. No app needed.` directly below it;
- large **COPY TO PHONE** panel: retrieve the selected item onto the phone;
- large **COPY INSIDE** panel: send text, images or files from the phone into ClipBridge;
- one compact **CURRENT ITEM** card with fixed, non-overlapping title/type/status regions;
- persistent Home / History / Settings navigation;
- Wi-Fi name and battery percentage in a compact top status bar.

The COPY/PASTE panels are informational, not touchscreen buttons. The physical NFC zones remain the primary interaction.

### History UI: no crowded action buttons

- Four recent items fit in a fixed, non-scrollable list.
- A row tap selects the item and returns directly to Home.
- Filters compact the visible rows instead of leaving holes.
- `Browse full history` opens the existing one-item browser for detailed preview/navigation.
- The 50-item RAM limit and microSD history index are preserved.

### Backlight auto-off / wake

ClipBridge has no hard OFF button, so v11 only powers down the LCD backlight:

- backlight turns off after **30 seconds** with no touch/NFC activity;
- ESP32, Wi-Fi AP/STA, web server, storage and NFC remain running;
- touching the display wakes it;
- the first wake touch is consumed so it cannot accidentally activate a control;
- either ST25DV GPO can wake the display when an NFC RF-field event is detected.

Change `DISPLAY_BACKLIGHT_TIMEOUT_MS` in `src/clipbridge_config.h` if another timeout is preferred.

### Battery display

- The UI starts with a stable **95%** fallback instead of `--%`.
- If `VBAT_SENSE` returns a plausible 1-cell LiPo voltage, the firmware uses a smoothed, non-linear voltage-to-percent estimate.
- If the ADC/divider reading is implausible, the UI keeps the 95% fallback.

The value is an estimate because the hardware does not include a dedicated fuel-gauge IC.

## COPY / PASTE behavior

- **COPY TO PHONE** tag opens `/copy`: the phone receives/copies the item currently selected on ClipBridge.
- **COPY INSIDE** tag opens `/paste`: ClipBridge receives text, links, images, PDFs and other files from the phone.
- The two physical NFC bus assignments were corrected after testing the assembled board so the upper COPY zone and lower PASTE zone now match what the user sees on screen.
- Image/file streaming to microSD, Wi-Fi credential persistence, DNS/AP fallback and NFC NDEF URL programming are preserved from v8.

## Notes

The microSD history index remains named `/history_v8.idx` on purpose so a board upgraded from v8 keeps its existing history.

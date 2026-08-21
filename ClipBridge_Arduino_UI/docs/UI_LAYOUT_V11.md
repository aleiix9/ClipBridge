# UI layout v11 (240 x 320 portrait)

## Home

- y 0..25: status bar (Wi-Fi + SSID + battery)
- y 31..49: `ClipBridge` title
- y 54..68: `by Aleix Ferrer. No app needed.`
- y 76..136: `COPY TO PHONE` NFC guidance card
- y 143..203: `COPY INSIDE` NFC guidance card
- y 211..225: CURRENT ITEM label
- y 228..273: current-item card
- y 276..319: bottom navigation

The CURRENT ITEM card reserves independent fixed regions for icon, title, type pill and status line. The title/status regions stop before the type pill, so long values are clipped rather than drawn underneath another element.

## History

- y 0..25: status bar
- y 32: title
- y 58: one-line instruction
- y 77..104: filter tabs
- remaining content stays above the bottom navigation
- y 276..319: bottom navigation

COPY/PASTE semantics are defined from the phone/user point of view:

- COPY TO PHONE: phone receives the selected item from ClipBridge.
- COPY INSIDE: ClipBridge receives content from the phone.

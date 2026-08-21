# ClipBridge UI v11

- Home subtitle: `by Aleix Ferrer. No app needed.`
- Incoming action shortened to `COPY INSIDE`; `COPY TO PHONE` remains unchanged.
- Current-item text/link title shortened to `Ready`.
- Added dedicated physical action LED control:
  - COPY LED: GPIO2 (LED3 + R17 220R).
  - PASTE LED: GPIO48 (LED4 + R20 220R).
- NFC activity lights the LED beside the touched physical zone for about 3 seconds.
- Text COPY/PASTE activity lights the matching LED for about 3 seconds.
- File download/upload holds the matching LED ON for the complete transfer, then keeps it lit briefly afterwards.
- 30 s LCD-backlight timeout and touch/NFC wake are retained.
- Battery percentage behavior and 95% fallback are retained.

# ClipBridge Pinout

This file is the firmware pin map derived from the EasyEDA MCU screenshot.

| Block | Net | GPIO |
|---|---|---:|
| Touch | TOUCH_SCL | 4 |
| Touch | TOUCH_SDA | 5 |
| Touch | TOUCH_INT | 6 |
| Display | DISP_BL | 7 |
| Touch | TOUCH_RST | 15 |
| NFC PASTE (physical lower zone) | NFC_PASTE_GPO | 16 |
| NFC PASTE (physical lower zone) | I2C_PASTE_SDA | 17 |
| NFC PASTE (physical lower zone) | I2C_PASTE_SCL | 18 |
| Display | DISP_RST | 8 |
| USB | USB_DN | 19 |
| USB | USB_DP | 20 |
| Display | DISP_DC | 9 |
| Display | DISP_CS | 10 |
| Display | DISP_MOSI | 11 |
| Display | DISP_SCLK | 12 |
| microSD | SD_D0 | 13 |
| microSD | SD_CMD | 14 |
| microSD | SD_CLK | 21 |
| microSD | SD_D3 | 47 |
| Battery | VBAT_SENSE | 1 |
| UART | TXD0 | 43 |
| UART | RXD0 | 44 |
| microSD | SD_D2 | 42 |
| microSD | SD_D1 | 41 |
| NFC COPY (physical upper zone) | NFC_COPY_GPO | 40 |
| NFC COPY (physical upper zone) | I2C_COPY_SDA | 39 |
| NFC COPY (physical upper zone) | I2C_COPY_SCL | 38 |
| Boot | BOOT | 0 |
| COPY LED | LED3 through R17 (220R) | 2 |
| PASTE LED | LED4 through R20 (220R) | 48 |

Notes:

- Latest schematic verification: COPY LED is GPIO2 and PASTE LED is GPIO48. Both are active HIGH.
- Firmware lights the matching LED for ~3 s on text/NFC actions and keeps it ON through file transfers.
- GPIO0 must be available as BOOT/download mode.
- EN must be available as reset.

- NFC bus naming was corrected in UI v10 and retained in v11 after physical verification on the assembled board: the upper COPY antenna is GPIO40/39/38 and the lower PASTE antenna is GPIO16/17/18.

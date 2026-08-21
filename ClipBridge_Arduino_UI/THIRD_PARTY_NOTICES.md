# Third-party notices

## LVGL

This project includes LVGL v9.2.x source code under:

`src/lvgl`

LVGL is distributed under the MIT License.  
The original LVGL license file is included as:

`src/lvgl/LICENCE.txt`

Project: https://github.com/lvgl/lvgl

## Hardware reference sources

The display and touch drivers were written for:

- Waveshare 2.8inch Capacitive Touch LCD
- ST7789 display controller
- CST328 touch controller

The CST328 packet parsing follows the register layout used in Waveshare's public example code.

## ST25DV

The included minimal ST25DV driver writes a standard NFC Forum Type 5 NDEF URI to the ST25DV04KC user EEPROM through I2C. It is an independent minimal implementation, not ST's X-CUBE middleware.

# ClipBridge Desktop and Firmware v12

ClipBridge v12 keeps the known-working v11 firmware intact and adds a local Windows desktop app plus a small HTTP API extension for desktop use.

## Project Layout

- `ClipBridge_Arduino_UI_v11/` is the untouched known-working firmware.
- `ClipBridge_Arduino_UI_v12/` is copied from v11 and contains only the v12 firmware changes.
- `desktop/` contains the Tauri 2, React, TypeScript, and Rust Windows desktop application.
- `CHANGELOG_v12.md` summarizes the v12 changes.

## Windows Desktop App

Requirements:

- Windows 10/11.
- Node.js and npm.
- Rust stable toolchain.
- Tauri 2 prerequisites for Windows, including Microsoft Visual Studio Build Tools with the C++ workload and WebView2.

Development:

```powershell
cd .\desktop
npm install
npm run tauri:dev
```

Frontend checks/build:

```powershell
cd .\desktop
npm run typecheck
npm run build
```

Rust checks:

```powershell
cd .\desktop\src-tauri
cargo check
```

Production installer/build:

```powershell
cd .\desktop
npm run tauri:build
```

Generated Windows bundles are normally written under:

```text
desktop/src-tauri/target/release/bundle/
```

The exact MSI/NSIS subfolder depends on the installed Tauri bundler target support.

## Desktop Discovery

The app is local-only. It sends no telemetry and uses no cloud service.

Discovery order:

1. `http://clipbridge.local`
2. last successful ClipBridge address
3. mDNS `_http._tcp.local.` browse result containing `clipbridge`
4. `http://192.168.4.1`

No `/24` LAN scan is performed by default.

## Network Modes

DIRECT:

```text
Windows PC
   |
 Wi-Fi
   |
ClipBridge
192.168.4.1
```

LAN:

```text
Windows PC
 Ethernet/Wi-Fi
      |
    Router
      |
    Wi-Fi
      |
 ClipBridge
```

A Windows PC connected by Ethernet can communicate with ClipBridge when ClipBridge is connected to the same LAN by Wi-Fi.

## Firmware v12

Open `ClipBridge_Arduino_UI_v12/ClipBridge_Arduino_UI_v12.ino` in Arduino IDE.

Use the same board target and libraries already required by v11:

- ESP32-S3 Arduino core.
- LVGL as bundled in `src/lvgl`.
- ESP32 Arduino `WiFi`, `WebServer`, `DNSServer`, `ESPmDNS`, `SD_MMC`, `Preferences`.
- Existing local modules in `src/` for ST7789 display, CST328 touch, ST25DV NFC, storage, and UI.

Compile/upload procedure:

1. Keep `ClipBridge_Arduino_UI_v11/` unchanged as the rollback firmware.
2. Open the v12 sketch folder.
3. Select the same ESP32-S3 board settings used for v11.
4. Compile.
5. Upload to the device.

## New v12 HTTP API

Existing v11 routes are preserved:

- `/`
- `/copy`
- `/api/copy-activity`
- `/paste` GET/POST
- `/upload`
- `/download`
- `/media`
- `/wifi` GET/POST
- `/api/status`
- captive portal probe routes

New or extended API:

- `GET /api/status`
- `GET /api/items`
- `GET /api/items/{id}`
- `GET /api/items/{id}/download`
- `POST /api/text`
- `POST /api/upload`
- `DELETE /api/items/{id}`

`/api/status` does not expose saved Wi-Fi passwords.

## mDNS

v12 starts:

```text
clipbridge.local
_http._tcp port 80
```

If mDNS fails, the firmware logs the issue and continues booting. The AP address `192.168.4.1` remains available while the AP is running.

## Runtime Behavior

- Text and uploads reuse the existing history/storage paths.
- File and image downloads by ID do not depend on the currently selected physical UI item.
- Uploads and receive actions use GPIO48.
- Downloads use GPIO2.
- GPIO assignments remain:
  - GPIO2 = COPY LED
  - GPIO48 = PASTE / COPY INSIDE LED

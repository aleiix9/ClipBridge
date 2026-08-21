# ClipBridge v12 Changelog

## Preserved

- `ClipBridge_Arduino_UI_v11/` remains untouched.
- Physical display branding and wording remain from v11.
- Existing AP+STA Wi-Fi architecture remains.
- Existing mobile web UI and routes remain.
- GPIO2 remains the COPY LED.
- GPIO48 remains the PASTE / COPY INSIDE LED.
- Backlight timeout, touch wake, NFC wake, NFC setup, SD storage, history loading, upload storage, and battery fallback behavior remain in place.

## Firmware

- Created `ClipBridge_Arduino_UI_v12/` from v11.
- Renamed the Arduino sketch to `ClipBridge_Arduino_UI_v12.ino`.
- Added non-blocking mDNS startup for `clipbridge.local`.
- Advertised `_http._tcp` on port 80 when mDNS starts.
- Extended `GET /api/status` with desktop fields while preserving existing status fields.
- Added `GET /api/items`.
- Added `GET /api/items/{id}`.
- Added `GET /api/items/{id}/download`.
- Added `POST /api/text`.
- Added `POST /api/upload` using the existing upload receive path.
- Added `DELETE /api/items/{id}` with SD file cleanup for file/image items.
- Added compact history index rewriting after individual deletion.

## Desktop

- Added `desktop/` Tauri 2 app.
- Added React/TypeScript UI.
- Added Rust backend commands for discovery, status, item list/detail, text send, file upload, file download, and delete.
- Added local discovery without subnet scanning.
- Added last-known address persistence with no sensitive data.
- Added native file picker and save dialog.
- Added native file drag/drop path handling.
- Added Windows clipboard copy support.
- Added system browser opening for links.

## Verification Notes

- v12 source preserves v11 pin definitions and AP+STA setup.
- Build checks should be run from `desktop/` and `desktop/src-tauri/`.
- Arduino compile depends on the local Arduino ESP32-S3 environment being installed.

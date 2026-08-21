# Build ClipBridge on macOS M2

These commands build the Apple Silicon `.app` and `.dmg` from a Mac.

## Requirements

- macOS on Apple Silicon.
- Xcode Command Line Tools:

```bash
xcode-select --install
```

- Node.js 20+.
- Rust:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
rustup target add aarch64-apple-darwin
```

## Build

From the `desktop` folder:

```bash
npm install
npm run tauri:build -- --target aarch64-apple-darwin --bundles app,dmg
```

The output will be in:

```text
src-tauri/target/aarch64-apple-darwin/release/bundle/
```

## First Open

If macOS blocks the app because it is unsigned, right-click `ClipBridge.app`, choose `Open`, then confirm.

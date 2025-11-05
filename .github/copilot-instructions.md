## Quick orientation

- This is a Pebble smartwatch app (native C + companion JS). The native app lives in `src/c/` (C sources) and the JS companion code in `src/pkjs/` (packaged into the watch bundle).
- The build is driven by the Pebble SDK/Waf `wscript` located at the project root (it calls `ctx.load('pebble_sdk')`). The JS bundle entrypoint is `src/pkjs/index.js` (see `wscript` `js_entry_file`).
- The goal of the app is to query Markdown files from a Nextcloud server via WebDAV and display checklists on the watch.
- Only lines starting with `- [ ]` or `- [x]` are treated as checklist items. Other lines are ignored.
- The app uses Basic Authentication with an app password for Nextcloud access.
- The goal of the app is to allow checking off items on the watch - only unchecked items should be shown.

## Build & run (developer workflows)

- Primary build: use the Pebble CLI in the project root. Typical commands you will use:

  - `pebble build` — builds native C and bundles JS into a `.pbw` in `build/`
  - `pebble install --emulator <platform>` — installs to the emulator (e.g. `basalt`) if you need to test quickly
  - `pebble logs` — stream native and JS logs for debugging

- The `wscript` handles multi-platform builds (aplite/basalt/chalk/diorite/emery/flint) and supports optional worker binaries via a `worker_src/` folder.

## Where code lives (big picture)

- Native watch app (UI, sensors): `src/c/` — C files compiled per platform (example: `src/c/workout.c`). Naming conventions: `s_` prefix for static global state and `prv_` prefix for internal functions.
- Companion/packaged JS: `src/pkjs/index.js` — performs network/XHR calls and may send messages to the native app. The bundle entry is referenced in `wscript` with `js_entry_file='src/pkjs/index.js'`.
- Generated and build artifacts: `build/` — contains per-platform bundles, generated `message_keys` headers and the final `.pbw`. Do not edit files under `build/` directly.

## Interop & integration patterns

- AppMessage / message keys: message key headers are generated into `include/message_keys.auto.h` and `src/message_keys.auto.c`. If you need to add new keys, add/update the JS message-keys file (project may use a `js/message_keys.json` convention) and rebuild so the auto-generated C header gets updated.
- Typical flow: JS performs network/XHR (see `src/pkjs/index.js`), then uses `Pebble.sendAppMessage(...)` (or similar) to forward data to C. On the C side handle messages using Pebble's AppMessage APIs and map keys via the auto-generated header.

## Project-specific conventions and examples

- Naming: `s_` for static globals, `prv_` for private functions (C). Example: `s_window`, `prv_init()` in `src/c/workout.c`.
- UI: uses Pebble SDK layers (TextLayer, Window). Time handling uses `tick_timer_service_subscribe(MINUTE_UNIT, ...)` in the C code.
- JS networking: `src/pkjs/index.js` contains an XMLHttpRequest-based example showing basic auth and how the code logs status and could forward contents to the watch.

## Files to inspect when making changes

- `wscript` — build config and JS/C bundle rules (important when adding JS entrypoints or worker binaries)
- `src/pkjs/index.js` — companion code; network and message-sending patterns live here
- `src/c/*.c` — native UI and message handling (example: `src/c/workout.c`)
- `include/message_keys.auto.h` and `src/message_keys.auto.c` — generated message key mapping used by native code

## Tips for the AI assistant

- Prefer editing source files under `src/` do not edit the generated files under `build/`.
- When changing message keys, update the JS-side JSON (if present) and run a build to refresh generated headers before editing C code that includes them.
- Use `pebble logs` to validate APP_LOG messages from C and console logs from JS (good first step after small changes).
- If adding a companion network request, mirror the pattern in `src/pkjs/index.js`: handle `onload`, `onerror`, `onreadystatechange`, and call into the app via `Pebble.sendAppMessage`.

## Project-specific details

- Where are the canonical message key definitions (if not `js/message_keys.json`)?
- Preferred emulator/platform to run during development: diorite
- Prefer running the emulator with very verbose logging: `pebble install --emulator diorite -vvv` to enable the logs from PebbleKit JS and see network request logs.
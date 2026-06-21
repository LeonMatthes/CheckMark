# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this app does

CheckMark is a Pebble smartwatch app that fetches a Markdown file from a Nextcloud server via WebDAV and displays its checklist items. Only lines matching `- [ ] <text>` are shown. Checking an item on the watch toggles the line to `- [x]` and uploads the updated file back to the server via PUT. Authentication uses HTTP Basic Auth with a Nextcloud app password.

## Build & run

```
pebble build                               # compile C + bundle JS → build/CheckMark.pbw
./pebble.sh install --emulator emery -vvv  # install to emulator with verbose logs (preferred platform)
./pebble.sh logs                           # stream APP_LOG (C) and console.log (JS)
```

`pebble.sh` is a wrapper that sets up `LD_LIBRARY_PATH` with local symlinks for `libsndio.so.7` and `libbz2.so.1.0` (missing on Fedora) before forwarding all arguments to `pebble`.

There are no unit tests. Validation is done by running the emulator.

## Architecture

The app is split into two halves that communicate via Pebble's AppMessage API:

**Companion JS** (`src/pkjs/`):
- `index.js` — all network logic: fetches the document via XHR GET, parses checklist items, streams items to the watch one-by-one, and handles item-check events by patching the document lines and uploading via PUT.
- `config.js` — Clay config schema for the in-app settings UI (WebDAV URL, username, password). Settings are persisted to `localStorage` and loaded at runtime.

**Native C** (`src/c/`):
- `main.c` — window setup, `SimpleMenuLayer` for the checklist, AppMessage inbox/outbox handlers. The JS streams items as `{ITEMS_COUNT, LIST_TITLE}` then `{ITEMS_INDEX, ITEMS_ITEM}` per item. When the last item arrives, `complete_list_update()` rebuilds the menu layer with a slide-in animation. Selecting an item sends `{ITEM_CHECKED}` or `{ITEM_UNCHECKED}` back to JS.
- `statusbar.c/.h` — custom status bar: wraps Pebble's `StatusBarLayer`, adds an animated `TextLayer` that slides in/out for transient status messages, and a progress bar animation (`s_progress_layer`) that loops while JS is loading/uploading.

**Message keys** are defined in `package.json` under `pebble.messageKeys` and auto-generated into `message_keys.auto.h` / `message_keys.auto.c` at build time. Never edit those generated files.

## Conventions

- C naming: `s_` prefix for static file-scope globals, `prv_` prefix for private (static) functions.
- Color: use `PBL_IF_COLOR_ELSE(color, bw_fallback)` for color/B&W platform differences.
- Memory: items and the menu view array are heap-allocated and freed in `dealloc_items_and_view()`. Always call this before reallocating.
- AppMessage inbox size is 1024 bytes; keep individual messages small (one item per message).

## Key files

| File | Purpose |
|------|---------|
| `package.json` | App metadata + message key definitions |
| `wscript` | Waf build script; lists JS glob and entry point |
| `src/pkjs/index.js` | All network, parsing, and watch communication logic |
| `src/pkjs/config.js` | Clay settings schema |
| `src/c/main.c` | UI, menu layer, AppMessage handlers |
| `src/c/statusbar.c` | Animated status/progress bar implementation |

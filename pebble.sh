#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBS_DIR="$SCRIPT_DIR/.pebble-libs"

mkdir -p "$LIBS_DIR"

# libbz2.so.1.0 — Fedora ships it as libbz2.so.1.0.8
if [[ ! -e "$LIBS_DIR/libbz2.so.1.0" ]]; then
  ln -s /usr/lib64/libbz2.so.1.0.8 "$LIBS_DIR/libbz2.so.1.0"
fi

# libsndio.so.7 — not in Fedora repos; use Steam's copy as a stand-in
if [[ ! -e "$LIBS_DIR/libsndio.so.7" ]]; then
  ln -s "/home/leon/.local/share/Steam/steamapps/common/Prison Architect/lib64/libsndio.so.6.1" \
    "$LIBS_DIR/libsndio.so.7"
fi

LD_LIBRARY_PATH="$LIBS_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" pebble "$@"

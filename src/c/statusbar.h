#pragma once

#include <pebble.h>

// Initialize the clock UI and start tick updates. Pass the main Window so the
// clock layer can be attached to its root layer.
void status_bar_init(Window *window);

// Deinitialize the clock UI and stop tick updates.
void status_bar_deinit(void);

// Set a short status message shown at the top-left of the screen.
// The text is copied into an internal buffer; callers may pass temporary
// strings. If passed NULL, the status is set to an empty string.
void status_bar_set_status(const char *text);

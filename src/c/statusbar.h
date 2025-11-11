#pragma once

#include <pebble.h>

// Initialize the status bar UI and start tick updates. Pass the main Window so the
// layers can be attached to its root layer.
void status_bar_init(Window *window);

// Deinitialize the status bar and stop tick updates.
void status_bar_deinit(void);

// Set a short status message shown at the top of the screen.
// This will animate in/out as needed and covers the clock while visible.
// The text is copied into an internal buffer; callers may pass temporary
// strings. If passed NULL or an empty string, the status becomes invisible.
void status_bar_set_status(const char *text);

void status_bar_set_progressing(bool progressing);

Layer *status_bar_get_layer();
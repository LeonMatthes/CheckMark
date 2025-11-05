#pragma once

#include <pebble.h>

// Initialize the clock UI and start tick updates. Pass the main Window so the
// clock layer can be attached to its root layer.
void clock_init(Window *window);

// Deinitialize the clock UI and stop tick updates.
void clock_deinit(void);

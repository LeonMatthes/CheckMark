#include <pebble.h>
#include <string.h>
#include <time.h>

#include "statusbar.h"

static TextLayer *s_status_layer = NULL;
static StatusBarLayer *s_status_bar_layer = NULL;
// Internal buffer for status text so callers don't need to keep strings alive.
static char s_status_buffer[32];

void status_bar_init(Window *window) {
  if (!window)
    return;

  Layer *window_layer = window_get_root_layer(window);

  // Create the status bar at the top of the window
  s_status_bar_layer = status_bar_layer_create();
  status_bar_layer_set_separator_mode(s_status_bar_layer, StatusBarLayerSeparatorModeDotted);
  status_bar_layer_set_colors(s_status_bar_layer, GColorClear, GColorBlack);

  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar_layer));

  GRect bounds = layer_get_bounds(status_bar_layer_get_layer(s_status_bar_layer));

  // Create the status text layer at the top-left of the window
  s_status_layer = text_layer_create(
      GRect(0, 0, bounds.size.w, bounds.size.h - 1)); // Leave space for the separator line
  // Default status
  strncpy(s_status_buffer, "Loading...", sizeof(s_status_buffer) - 1);
  s_status_buffer[sizeof(s_status_buffer) - 1] = '\0';
  text_layer_set_text(s_status_layer, s_status_buffer);
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_status_layer));
}

void status_bar_deinit(void) {
  if (s_status_layer) {
    layer_remove_from_parent(text_layer_get_layer(s_status_layer));
    text_layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
  if (s_status_bar_layer) {
    status_bar_layer_destroy(s_status_bar_layer);
    s_status_bar_layer = NULL;
  }
}

void status_bar_set_status(const char *text) {
  if (!s_status_layer)
    return;
  if (!text)
    text = "";
  // Copy into internal buffer and update the layer
  strncpy(s_status_buffer, text, sizeof(s_status_buffer) - 1);
  s_status_buffer[sizeof(s_status_buffer) - 1] = '\0';
  text_layer_set_text(s_status_layer, s_status_buffer);
}

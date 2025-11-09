#include <pebble.h>
#include <string.h>
#include <time.h>

#include "statusbar.h"

const int CLOCK_WIDTH = 30;
const int SIDE_MARGIN = 5;

static TextLayer *s_clock_layer = NULL;
static TextLayer *s_status_layer = NULL;
// Internal buffer for status text so callers don't need to keep strings alive.
static char s_status_buffer[32];

static void prv_update_time(struct tm *tick_time) {
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M",
           tick_time);
  text_layer_set_text(s_clock_layer, s_buffer);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)units_changed;
  prv_update_time(tick_time);
}

void status_bar_init(Window *window) {
  if (!window)
    return;

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the status text layer at the top-left of the window
  s_status_layer = text_layer_create(
      GRect(SIDE_MARGIN, 2, bounds.size.w - CLOCK_WIDTH - SIDE_MARGIN * 2, 18));
  // Default status
  strncpy(s_status_buffer, "Loading...", sizeof(s_status_buffer) - 1);
  s_status_buffer[sizeof(s_status_buffer) - 1] = '\0';
  text_layer_set_text(s_status_layer, s_status_buffer);
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentLeft);

  // Create the clock text layer at the top-right of the window. Make it
  // occupy the right half to avoid overlapping the status text.
  s_clock_layer = text_layer_create(
      GRect(bounds.size.w - CLOCK_WIDTH - SIDE_MARGIN, 2, CLOCK_WIDTH, 18));
  text_layer_set_text(s_clock_layer, "00:00");
  text_layer_set_font(s_clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_clock_layer, GTextAlignmentRight);

  // Update immediately so the clock shows the current time
  time_t temp = time(NULL);
  struct tm *now = localtime(&temp);
  prv_update_time(now);

  // Add both layers to the window
  if (s_status_layer) {
    layer_add_child(window_layer, text_layer_get_layer(s_status_layer));
  }
  layer_add_child(window_layer, text_layer_get_layer(s_clock_layer));

  // Subscribe to minute ticks
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

void status_bar_deinit(void) {
  if (s_clock_layer) {
    layer_remove_from_parent(text_layer_get_layer(s_clock_layer));
    text_layer_destroy(s_clock_layer);
    s_clock_layer = NULL;
  }
  if (s_status_layer) {
    layer_remove_from_parent(text_layer_get_layer(s_status_layer));
    text_layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
  tick_timer_service_unsubscribe();
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

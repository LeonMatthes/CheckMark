#include <pebble.h>
#include <time.h>
#include <string.h>

#include "clock.h"

static TextLayer *s_clock_layer = NULL;

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

void clock_init(Window *window) {
  if (!window) return;

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the clock text layer at the top of the window
  s_clock_layer = text_layer_create(GRect(0, 5, bounds.size.w, 14));
  text_layer_set_text(s_clock_layer, "00:00");
  text_layer_set_font(s_clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_clock_layer, GTextAlignmentCenter);

  // Update immediately so it shows up right away
  time_t temp = time(NULL);
  struct tm *now = localtime(&temp);
  prv_update_time(now);

  layer_add_child(window_layer, text_layer_get_layer(s_clock_layer));

  // Subscribe to minute ticks
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

void clock_deinit(void) {
  if (s_clock_layer) {
    layer_remove_from_parent(text_layer_get_layer(s_clock_layer));
    text_layer_destroy(s_clock_layer);
    s_clock_layer = NULL;
  }
  tick_timer_service_unsubscribe();
}

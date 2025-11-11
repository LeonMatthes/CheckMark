#include <pebble.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

#include "statusbar.h"

static StatusBarLayer *s_status_bar_layer = NULL;
// Internal buffer for status text so callers don't need to keep strings alive.
static char s_status_buffer[32] = "";
static TextLayer *s_status_layer = NULL;
static PropertyAnimation *s_status_anim = NULL;
static bool s_status_visible = false;
// Animation duration (ms)
#define STATUS_ANIM_DURATION 200

static Layer *s_progress_layer = NULL;
static PropertyAnimation *s_progress_anim = NULL;
bool s_progressing = false;

static void prv_anim_stopped(Animation *animation, bool finished, void *context);
static void prv_animate_layer_to(Layer *layer, GRect to_frame, uint32_t delay_ms);
static void start_progress_anim();

static void draw_progress_layer(Layer *layer, GContext *ctx) {
  // Example progress drawing (replace with actual progress logic)
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorOrange, GColorBlack));
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void finish_progress_anim(Animation *animation, bool finished, void *context) {
  if (finished && s_progressing) {
    start_progress_anim();
  }
  else {
    if (s_progress_anim) {
      property_animation_destroy(s_progress_anim);
      s_progress_anim = NULL;
    }
  }
}

static void start_progress_anim() {
  if (!s_progress_layer)
    return;

  // Cancel previous animation if running
  if (s_progress_anim) {
    property_animation_destroy(s_progress_anim);
    s_progress_anim = NULL;
  }

  GRect bar_bounds = layer_get_bounds(status_bar_layer_get_layer(s_status_bar_layer));
  GRect from = GRect(-bar_bounds.size.w / 3, bar_bounds.size.h - 2, bar_bounds.size.w / 3, 2);
  GRect to = GRect(bar_bounds.size.w, bar_bounds.size.h - 2, bar_bounds.size.w / 4, 2);

  s_progress_anim = property_animation_create_layer_frame(s_progress_layer, &from, &to);
  animation_set_duration((Animation *)s_progress_anim, 1000);
  animation_set_curve((Animation *)s_progress_anim, AnimationCurveEaseInOut);
  
  // Repeat indefinitely if still progressing
  AnimationHandlers handlers = {
    .stopped = finish_progress_anim
  };
  animation_set_handlers((Animation *)s_progress_anim, handlers, NULL);
  
  animation_schedule((Animation *)s_progress_anim);
}

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
      GRect(0, -bounds.size.h, bounds.size.w, bounds.size.h - 1)); // Leave space for the separator line
  text_layer_set_font(s_status_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_background_color(s_status_layer, PBL_IF_COLOR_ELSE(GColorRajah, GColorBlack));
  text_layer_set_text_color(s_status_layer, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_text(s_status_layer, s_status_buffer);

  layer_add_child(window_layer, text_layer_get_layer(s_status_layer));

  s_progress_layer = layer_create(GRect(-bounds.size.w / 3, bounds.size.h - 2, bounds.size.w / 3, 2));
  layer_set_update_proc(s_progress_layer, draw_progress_layer);
  layer_add_child(window_layer, s_progress_layer);
}

void status_bar_deinit(void) {
  // Cancel any running animation
  if (s_status_anim) {
    property_animation_destroy(s_status_anim);
    s_status_anim = NULL;
  }
  if (s_status_layer) {
    text_layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
  if (s_status_bar_layer) {
    status_bar_layer_destroy(s_status_bar_layer);
    s_status_bar_layer = NULL;
  }
  if (s_progress_anim) {
    property_animation_destroy(s_progress_anim);
    s_progress_anim = NULL;
  }
  if (s_progress_layer) {
    layer_destroy(s_progress_layer);
    s_progress_layer = NULL;
  }
}

void status_bar_set_status(const char *text) {
  if (!s_status_layer)
    return;
  if (!text)
    text = "";

  // Decide visibility based on whether the status text is empty
  bool should_be_visible = strlen(text) > 0;
  if (should_be_visible != s_status_visible) {
    // Animate the text layer in or out from the top
    Layer *layer = text_layer_get_layer(s_status_layer);
    GRect bounds = layer_get_frame(layer);
    GRect on_screen = GRect(0, 0, bounds.size.w, bounds.size.h);
    GRect off_screen = GRect(0, -bounds.size.h, bounds.size.w, bounds.size.h);
    if (should_be_visible) {
      // animate from off_screen -> on_screen
      prv_animate_layer_to(layer, on_screen, 0 /*no delay when showing the status*/);
    } else {
      layer_set_frame(layer, on_screen); // Ensure starting from on_screen position
      // animate from on_screen -> off_screen
      prv_animate_layer_to(layer, off_screen, STATUS_ANIM_DURATION * 2 /*delay when hiding the status to allow reading time*/);
    }
    s_status_visible = should_be_visible;
  }
  // Copy into internal buffer only if the status should be visible, to retain the last message during the fade-out animation
  if (should_be_visible) {
    strncpy(s_status_buffer, text, sizeof(s_status_buffer) - 1);
    s_status_buffer[sizeof(s_status_buffer) - 1] = '\0';
    text_layer_set_text(s_status_layer, s_status_buffer);
  }
}

static void prv_animate_layer_to(Layer *layer, GRect to_frame, uint32_t delay_ms) {
  // Cancel previous animation if running
  if (s_status_anim) {
    property_animation_destroy(s_status_anim);
    s_status_anim = NULL;
  }
  GRect from = layer_get_frame(layer);
  s_status_anim = property_animation_create_layer_frame(layer, &from, &to_frame);
  animation_set_duration((Animation *)s_status_anim, STATUS_ANIM_DURATION);
  animation_set_curve((Animation *)s_status_anim, AnimationCurveEaseInOut);
  animation_set_delay((Animation*) s_status_anim, delay_ms);
  // Clean up when finished
  AnimationHandlers handlers = {.stopped = prv_anim_stopped};
  animation_set_handlers((Animation *)s_status_anim, handlers, NULL);
  animation_schedule((Animation *)s_status_anim);
}

static void prv_anim_stopped(Animation *animation, bool finished, void *context) {
  if (s_status_anim) {
    property_animation_destroy(s_status_anim);
    s_status_anim = NULL;
  }
}

void status_bar_set_progressing(bool progressing)
{
  if (progressing && !s_progress_anim) {
    start_progress_anim();
  }
  s_progressing = progressing;
}

Layer *status_bar_get_layer() {
  return status_bar_layer_get_layer(s_status_bar_layer);
}
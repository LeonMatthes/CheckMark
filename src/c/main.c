#include <pebble.h>
#include <string.h>

#include "message_keys.auto.h"
#include "statusbar.h"

#define CELL_HEIGHT 22

static Window *s_window;
static MenuLayer *s_menu_layer;
static char s_menu_title[64] = "Checklist";

typedef struct {
  bool checked;
  char *label;
} TodoItem;

typedef struct {
  char title[64];
  int  item_count;
} MenuSection;

static TodoItem   *s_items       = NULL;
static int         s_num_items   = 0;
static MenuSection *s_sections   = NULL;
static int         s_num_sections = 0;
static GRect s_menu_bounds;
static GBitmap *s_checked_icon;

static void complete_list_update();

// Clean up PropertyAnimation when it finishes
static void prv_property_animation_stopped(Animation *anim, bool finished,
                                           void *context) {
  if (finished) {
    property_animation_destroy((PropertyAnimation *)anim);
  }
}

static bool prv_send_item_update(int index, bool checked) {
  DictionaryIterator *out_iter;
  AppMessageResult res = app_message_outbox_begin(&out_iter);
  if (res != APP_MSG_OK || out_iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)res);
    return false;
  }

  uint32_t key = checked ? MESSAGE_KEY_ITEM_CHECKED : MESSAGE_KEY_ITEM_UNCHECKED;
  dict_write_int(out_iter, key, &index, sizeof(index), true);
  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send action outbox: %d", (int)res);
    status_bar_set_status("Failed to send!");
    return false;
  }
  return true;
}

static void prv_item_selected(int index, void *ctx) {
  if (index < 0 || index >= s_num_items)
    return;

  bool new_checked_state = !s_items[index].checked;
  if (!prv_send_item_update(index, new_checked_state)) {
    return;
  }

  s_items[index].checked = new_checked_state;
  layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
}

// --- MenuLayer callbacks ---

static uint16_t prv_get_num_sections(MenuLayer *layer, void *ctx) {
  return s_num_sections > 0 ? (uint16_t)s_num_sections : 1;
}

static uint16_t prv_get_num_rows(MenuLayer *layer, uint16_t section_index, void *ctx) {
  if (s_num_sections == 0) return (uint16_t)s_num_items;
  if (section_index >= (uint16_t)s_num_sections) return 0;
  return (uint16_t)s_sections[section_index].item_count;
}

static int prv_global_index(uint16_t section, uint16_t row) {
  int base = 0;
  for (int i = 0; i < (int)section; i++) base += s_sections[i].item_count;
  return base + (int)row;
}

static int16_t prv_cell_height(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  return CELL_HEIGHT;
}

static int16_t prv_header_height(MenuLayer *layer, uint16_t section_index, void *ctx) {
  if (s_num_sections > 0 && section_index < (uint16_t)s_num_sections
      && s_sections[section_index].title[0] == '\0') {
    return 0;
  }
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer,
                            uint16_t section_index, void *cb_ctx) {
  const char *title = (s_num_sections > 0 && section_index < (uint16_t)s_num_sections)
    ? s_sections[section_index].title
    : s_menu_title;
  if (title[0] == '\0') return;

  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  graphics_draw_text(ctx, title,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(4, 0, bounds.size.w - 8, bounds.size.h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *idx, void *cb_ctx) {
  int global = (s_num_sections > 0)
    ? prv_global_index(idx->section, idx->row)
    : (int)idx->row;
  if (global < 0 || global >= s_num_items) return;
  TodoItem *item = &s_items[global];
  GRect bounds = layer_get_bounds(cell_layer);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  int horizontal_spacing = 4;
  int x = horizontal_spacing;
  if (item->checked && s_checked_icon) {
    GRect icon_bounds = gbitmap_get_bounds(s_checked_icon);
    int icon_y = (bounds.size.h - icon_bounds.size.h) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_checked_icon,
      GRect(x, icon_y, icon_bounds.size.w, icon_bounds.size.h));
    x += icon_bounds.size.w + horizontal_spacing;
  }

  graphics_draw_text(ctx, item->label, font,
    GRect(x, -2, bounds.size.w - x - horizontal_spacing, bounds.size.h),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
}

static void prv_select_click(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  int global = (s_num_sections > 0)
    ? prv_global_index(idx->section, idx->row)
    : (int)idx->row;
  prv_item_selected(global, ctx);
}

// ---------------------------

static void dealloc_items() {
  if (s_items) {
    for (int i = 0; i < s_num_items; i++) {
      free(s_items[i].label);
    }
    free(s_items);
    s_items = NULL;
  }
  s_num_items = 0;

  if (s_sections) {
    free(s_sections);
    s_sections = NULL;
  }
  s_num_sections = 0;
}

static void update_count(int count) {
  if (count < 0) count = 0;

  if (s_menu_layer) {
    layer_remove_from_parent(menu_layer_get_layer(s_menu_layer));
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  dealloc_items();

  if (count > 0) {
    s_items = calloc(count, sizeof(TodoItem));
    if (!s_items) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory for items");
      return;
    }
  }

  s_num_items = count;
  APP_LOG(APP_LOG_LEVEL_INFO, "Received count=%d", count);
}

static void add_item(int index, const char *item_text) {
  if (index < 0 || index >= s_num_items || !item_text) return;
  TodoItem *item = &s_items[index];
  free(item->label);
  int length = strlen(item_text);
  item->label = calloc(length + 1, sizeof(char));
  strncpy(item->label, item_text, length);
  item->checked = false;
}

static int prv_content_height() {
  int h = 0;
  if (s_num_sections == 0) {
    h += MENU_CELL_BASIC_HEADER_HEIGHT + s_num_items * CELL_HEIGHT;
  } else {
    for (int i = 0; i < s_num_sections; i++) {
      if (s_sections[i].title[0] != '\0') h += MENU_CELL_BASIC_HEADER_HEIGHT;
      h += s_sections[i].item_count * CELL_HEIGHT;
    }
  }
  return h;
}

static void complete_list_update() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Completed list update with %d items", s_num_items);

  if (s_menu_layer) {
    layer_remove_from_parent(menu_layer_get_layer(s_menu_layer));
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }

  int raw_h = prv_content_height();
  int content_h = raw_h < s_menu_bounds.size.h ? raw_h : s_menu_bounds.size.h;
  GRect menu_frame = GRect(s_menu_bounds.origin.x, s_menu_bounds.origin.y,
                           s_menu_bounds.size.w, content_h);

  s_menu_layer = menu_layer_create(menu_frame);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections  = prv_get_num_sections,
    .get_num_rows      = prv_get_num_rows,
    .get_cell_height   = prv_cell_height,
    .get_header_height = prv_header_height,
    .draw_header       = prv_draw_header,
    .draw_row          = prv_draw_row,
    .select_click      = prv_select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, s_window);
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu_layer, GColorPictonBlue, GColorBlack);
#endif

  // Slide the menu layer in from the left
  Layer *layer = menu_layer_get_layer(s_menu_layer);
  GRect from_frame = GRect(menu_frame.origin.x - menu_frame.size.w,
                           menu_frame.origin.y,
                           menu_frame.size.w, menu_frame.size.h);
  layer_set_frame(layer, from_frame);
  layer_add_child(window_get_root_layer(s_window), layer);

  PropertyAnimation *prop_anim =
      property_animation_create_layer_frame(layer, &from_frame, &menu_frame);
  Animation *anim = (Animation *)prop_anim;
  animation_set_duration(anim, 300);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_handlers(anim,
    (AnimationHandlers){.stopped = prv_property_animation_stopped}, NULL);
  animation_schedule(anim);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *t_title = dict_find(iter, MESSAGE_KEY_LIST_TITLE);
  if (t_title) {
    strncpy(s_menu_title, t_title->value->cstring, sizeof(s_menu_title) - 1);
    s_menu_title[sizeof(s_menu_title) - 1] = '\0';
  }

  Tuple *t_count = dict_find(iter, MESSAGE_KEY_ITEMS_COUNT);
  if (t_count) {
    update_count(t_count->value->int32);
  }

  Tuple *t_sc = dict_find(iter, MESSAGE_KEY_SECTION_COUNT);
  if (t_sc) {
    free(s_sections);
    s_num_sections = (int)t_sc->value->int32;
    s_sections = s_num_sections > 0
      ? calloc(s_num_sections, sizeof(MenuSection))
      : NULL;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received section_count=%d", s_num_sections);
  }

  Tuple *t_si = dict_find(iter, MESSAGE_KEY_SECTION_INDEX);
  Tuple *t_st = dict_find(iter, MESSAGE_KEY_SECTION_TITLE);
  Tuple *t_sn = dict_find(iter, MESSAGE_KEY_SECTION_ITEM_COUNT);
  if (t_si && t_st && t_sn && s_sections) {
    int idx = (int)t_si->value->int32;
    if (idx >= 0 && idx < s_num_sections) {
      strncpy(s_sections[idx].title, t_st->value->cstring,
              sizeof(s_sections[idx].title) - 1);
      s_sections[idx].title[sizeof(s_sections[idx].title) - 1] = '\0';
      s_sections[idx].item_count = (int)t_sn->value->int32;
      APP_LOG(APP_LOG_LEVEL_INFO, "Section %d: '%s' (%d items)",
              idx, s_sections[idx].title, s_sections[idx].item_count);
    }
  }

  Tuple *t_index = dict_find(iter, MESSAGE_KEY_ITEMS_INDEX);
  Tuple *t_item  = dict_find(iter, MESSAGE_KEY_ITEMS_ITEM);
  if (t_index && t_item) {
    int index = t_index->value->int32;
    add_item(index, t_item->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received item %d: %s", index, t_item->value->cstring);
    if (index == s_num_items - 1) {
      complete_list_update();
    }
  }

  Tuple *t_status = dict_find(iter, MESSAGE_KEY_SET_STATUS);
  if (t_status) {
    status_bar_set_status(t_status->value->cstring);
  }

  Tuple *t_progressing = dict_find(iter, MESSAGE_KEY_SET_PROGRESSING);
  if (t_progressing) {
    status_bar_set_progressing(t_progressing->value->int32 != 0);
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iter,
                                  AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);
  status_bar_set_status("Cannot reach phone!");
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, GColorBlack);
  status_bar_init(window);

  GRect bar_bounds = layer_get_bounds(status_bar_get_layer());
  s_menu_bounds = GRect(0, bar_bounds.size.h, bounds.size.w,
                        bounds.size.h - bar_bounds.size.h);
}

static void prv_window_unload(Window *window) {
  if (s_menu_layer) {
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  status_bar_deinit();
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(1024, 64);

  s_checked_icon = gbitmap_create_with_resource(RESOURCE_ID_CHECK_MARK);
}

static void prv_deinit(void) {
  gbitmap_destroy(s_checked_icon);
  window_destroy(s_window);
  dealloc_items();
}

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);
  app_event_loop();
  prv_deinit();
}

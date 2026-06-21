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

static TodoItem *s_items = NULL;
static int s_num_items = 0;
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

static uint16_t prv_get_num_rows(MenuLayer *layer, uint16_t section_index, void *ctx) {
  return (uint16_t)s_num_items;
}

static int16_t prv_cell_height(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  return CELL_HEIGHT;
}

static int16_t prv_header_height(MenuLayer *layer, uint16_t section_index, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell_layer,
                            uint16_t section_index, void *cb_ctx) {
  menu_cell_basic_header_draw(ctx, cell_layer, s_menu_title);
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer,
                         MenuIndex *idx, void *cb_ctx) {
  if (idx->row >= (uint16_t)s_num_items) return;
  TodoItem *item = &s_items[idx->row];
  GRect bounds = layer_get_bounds(cell_layer);

  int x = 4;
  if (item->checked && s_checked_icon) {
    GRect icon_bounds = gbitmap_get_bounds(s_checked_icon);
    int icon_y = (bounds.size.h - icon_bounds.size.h) / 2;
    graphics_draw_bitmap_in_rect(ctx, s_checked_icon,
      GRect(x, icon_y, icon_bounds.size.w, icon_bounds.size.h));
    x += icon_bounds.size.w + 4;
  }

  GRect text_rect = GRect(x, 0, bounds.size.w - x - 2, bounds.size.h);
  graphics_draw_text(ctx, item->label,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    text_rect,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
}

static void prv_select_click(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  prv_item_selected((int)idx->row, ctx);
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

static void complete_list_update() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Completed list update with %d items", s_num_items);

  if (s_menu_layer) {
    layer_remove_from_parent(menu_layer_get_layer(s_menu_layer));
    menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }

  s_menu_layer = menu_layer_create(s_menu_bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows    = prv_get_num_rows,
    .get_cell_height = prv_cell_height,
    .get_header_height = prv_header_height,
    .draw_header     = prv_draw_header,
    .draw_row        = prv_draw_row,
    .select_click    = prv_select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, s_window);
#ifdef PBL_COLOR
  menu_layer_set_highlight_colors(s_menu_layer, GColorPictonBlue, GColorBlack);
#endif

  // Slide the menu layer in from the left
  Layer *layer = menu_layer_get_layer(s_menu_layer);
  GRect from_frame = GRect(s_menu_bounds.origin.x - s_menu_bounds.size.w,
                           s_menu_bounds.origin.y,
                           s_menu_bounds.size.w, s_menu_bounds.size.h);
  layer_set_frame(layer, from_frame);
  layer_add_child(window_get_root_layer(s_window), layer);

  PropertyAnimation *prop_anim =
      property_animation_create_layer_frame(layer, &from_frame, &s_menu_bounds);
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

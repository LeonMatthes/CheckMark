#include <pebble.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "message_keys.auto.h"
#include "statusbar.h"

static Window *s_window;

// Simple checklist using SimpleMenuLayer with dummy items split into sections
static SimpleMenuLayer *s_menu_layer;
static SimpleMenuSection s_menu_sections[1];

// Combined struct to hold the data for a checklist/menu item.
// The SimpleMenuItem is kept in a separate contiguous view array
// (`s_menu_view`) required by the SimpleMenu API. `TodoItem` is the
// canonical source of truth for each item's label, title buffer and
#include <pebble.h>
#include <string.h>

static Window *s_window;

// Simple checklist using SimpleMenuLayer with dynamic items populated
// from the companion via AppMessage.
static SimpleMenuLayer *s_menu_layer;
static SimpleMenuSection s_menu_sections[1];
static char s_menu_title[64] = "Checklist";

typedef struct {
  bool checked;
  char *label;
} TodoItem;

static TodoItem *s_items = NULL;
static SimpleMenuItem *s_menu_view = NULL;
static int s_num_items = 0;
static GRect s_menu_bounds;
static GBitmap *s_checked_icon;

static void complete_list_update();

static void prv_update_item_titles() {
  for (int i = 0; i < s_num_items; i++) {
    TodoItem *item = &s_items[i];
    s_menu_view[i].title = item->label;
    s_menu_view[i].icon = item->checked ? s_checked_icon : NULL;
  }
}

static bool prv_send_item_update(int index, bool checked) {
  DictionaryIterator *out_iter;
  AppMessageResult res = app_message_outbox_begin(&out_iter);
  if (res != APP_MSG_OK || out_iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin outbox: %d", (int)res);
    return false;
  }

  uint32_t key =
      checked ? MESSAGE_KEY_ITEM_CHECKED : MESSAGE_KEY_ITEM_UNCHECKED;
  dict_write_int(out_iter, key, &index, sizeof(index), true);
  res = app_message_outbox_send();
  if (res != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to send action outbox: %d", (int)res);
    status_bar_set_status("Failed to send!");

    // Do not update the checklist state if the message failed to send, so that
    // there is a visual indication of the failure.
    return false;
  }
  return true;
}

static void prv_item_selected(int index, void *ctx) {
  if (index < 0 || index >= s_num_items)
    return;

  bool new_checked_state = !s_items[index].checked;
  if (!prv_send_item_update(index, new_checked_state)) {
    return; // Do not update the UI if sending the update failed
  }

  s_items[index].checked = new_checked_state;
  prv_update_item_titles();
  layer_mark_dirty(simple_menu_layer_get_layer(s_menu_layer));
}

static void dealloc_items_and_view() {
  if (s_items) {
    for (int i = 0; i < s_num_items; i++) {
      TodoItem *item = &s_items[i];
      if (item->label) {
        free(item->label);
        item->label = NULL;
      }
    }

    free(s_items);
    s_items = NULL;
  }
  if (s_menu_view) {
    free(s_menu_view);
    s_menu_view = NULL;
  }
}

static void update_count(int count) {
  if (count < 0)
    count = 0;

  // First, remove the existing menu layer if any
  if (s_menu_layer) {
    layer_remove_from_parent(simple_menu_layer_get_layer(s_menu_layer));
    simple_menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  dealloc_items_and_view();
  if (count > 0) {
    s_items = calloc(count, sizeof(TodoItem));
    s_menu_view = calloc(count, sizeof(SimpleMenuItem));
    if (!s_items || !s_menu_view) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to allocate memory for items");
      // this will free the other if only one allocation failed
      update_count(0);
      return;
    }
  }

  s_num_items = count;
  APP_LOG(APP_LOG_LEVEL_INFO, "Received count=%d", count);
}

static void add_item(int index, const char *item_text) {
  if (index >= 0 && index < s_num_items && item_text) {
    TodoItem *item = &s_items[index];

    if (item->label) {
      free(item->label);
      item->label = NULL;
    }
    int length = strlen(item_text);
    // calloc will automatically add the null terminator
    item->label = calloc(length + 1, sizeof(char));
    strncpy(item->label, item_text, length);
    item->checked = false;
  }
}

static void complete_list_update() {
  // Prepare view entries and titles
  for (int i = 0; i < s_num_items; i++) {
    s_menu_view[i] = (SimpleMenuItem){.title = s_items[i].label,
                                      .subtitle = (char *)0,
                                      .callback = prv_item_selected};
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Completed list update with %d items",
          s_num_items);

  prv_update_item_titles();

  // Recreate the menu layer with the new number of items
  if (s_menu_layer) {
    layer_remove_from_parent(simple_menu_layer_get_layer(s_menu_layer));
    simple_menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  s_menu_sections[0] = (SimpleMenuSection){
      .title = s_menu_title, .num_items = s_num_items, .items = s_menu_view};
  s_menu_layer = simple_menu_layer_create(s_menu_bounds, s_window,
                                          s_menu_sections, 1, NULL);
  layer_add_child(window_get_root_layer(s_window),
                  simple_menu_layer_get_layer(s_menu_layer));
}

// AppMessage inbox handler: receive one item at a time (with count/index/item)
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *t_title = dict_find(iter, MESSAGE_KEY_LIST_TITLE);
  if (t_title) {
    const char *title_text = t_title->value->cstring;
    strncpy(s_menu_title, title_text, sizeof(s_menu_title) - 1);
    s_menu_title[sizeof(s_menu_title) - 1] = '\0';
  }

  Tuple *t_count = dict_find(iter, MESSAGE_KEY_ITEMS_COUNT);
  if (t_count) {
    update_count(t_count->value->int32);
  }

  Tuple *t_index = dict_find(iter, MESSAGE_KEY_ITEMS_INDEX);
  Tuple *t_item = dict_find(iter, MESSAGE_KEY_ITEMS_ITEM);

  if (t_index && t_item) {
    int index = t_index->value->int32;
    const char *item_text = t_item->value->cstring;

    add_item(index, item_text);
    APP_LOG(APP_LOG_LEVEL_INFO, "Received item %d: %s", index, item_text);

    if (index == s_num_items - 1) {
      // Last item received; update the whole list
      complete_list_update();
    }
  }

  Tuple *t_status = dict_find(iter, MESSAGE_KEY_SET_STATUS);
  if (t_status) {
    const char *status_text = t_status->value->cstring;
    status_bar_set_status(status_text);
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

// Requires s_clock_layer to be valid
// Clock functions moved to clock.c (clock_init / clock_deinit)

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Clock at top
  // Initialize the clock UI (creates layer, sets text and subscribes ticks)
  status_bar_init(window);

  GRect bar_bounds = layer_get_bounds(status_bar_get_layer());
  s_menu_bounds = GRect(0, bar_bounds.size.h, bounds.size.w, bounds.size.h - bar_bounds.size.h);
}

static void prv_window_unload(Window *window) {
  if (s_menu_layer) {
    simple_menu_layer_destroy(s_menu_layer);
    s_menu_layer = NULL;
  }
  status_bar_deinit();
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });
  const bool animated = true;
  window_stack_push(s_window, animated);

  // Configure AppMessage to receive items from the companion JS
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  const uint32_t inbox_size = 1024;
  const uint32_t outbox_size = 64;
  app_message_open(inbox_size, outbox_size);

  s_checked_icon = gbitmap_create_with_resource(RESOURCE_ID_CHECK_MARK);
}

static void prv_deinit(void) {
  gbitmap_destroy(s_checked_icon);
  window_destroy(s_window);
  dealloc_items_and_view();
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p",
          s_window);

  app_event_loop();
  prv_deinit();
}

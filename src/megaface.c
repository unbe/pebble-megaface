#include <pebble.h>
#include "num2words.h"

static Window *s_main_window;

typedef struct _Facet {
  Layer *layer;
  GRect frame;
  TimeUnits changes_on;
  GColor8 color;
  void (*init)(struct _Facet* facet);
  void (*handle_tick)(struct _Facet* facet, struct tm *tick_time, TimeUnits units_changed);
  void (*handle_battery)(struct _Facet* facet, BatteryChargeState charge);
  void (*handle_bluetooth)(struct _Facet* facet, bool connected);

  // Graphic facets
  void (*draw)(struct _Facet* facet, Layer *layer, GContext *ctx);

  // Text facets
  TextLayer *textLayer;
  void (*get_text)(struct tm *t, char *buffer);
  char buffer[BUFFER_SIZE];
  const char* font_key;

  // Misc private data :)
  BatteryChargeState charge;
  bool last_bluetooth_state;
} Facet;

static void text_tick_handler(Facet* facet, struct tm *tick_time, TimeUnits units_changed) {
  facet->get_text(tick_time, facet->buffer);
  text_layer_set_text(facet->textLayer, facet->buffer);
}

static void text_init_layer(Facet* facet) {
  TextLayer *textLayer = facet->textLayer = text_layer_create(facet->frame);
  text_layer_set_background_color(textLayer, GColorClear);
  text_layer_set_text_alignment(textLayer, GTextAlignmentLeft);

  text_layer_set_text_color(textLayer, facet->color);
  text_layer_set_font(textLayer, fonts_get_system_font(facet->font_key));
  facet->layer = text_layer_get_layer(textLayer);
}

static void battery_draw_layer(Facet* facet, Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int blocks = (facet->charge.charge_percent - 1) / 10 + 1;
  GColor color;
  if (facet->charge.is_charging) {
    color = GColorGreen;
  } else if (blocks == 1) {
    color = GColorRed;
  } else if(blocks <= 3) {
    color = GColorYellow;
  } else {
    color = GColorWhite;
  }
  graphics_context_set_fill_color(ctx, color);

  int gap = 2;
  int block_step = bounds.size.w / 10;
  GSize block_sz = { .w = block_step - gap, .h = bounds.size.h };

  for (int i = 0; i < blocks; i++) {
    GRect block = {{gap + bounds.origin.x + block_step * i, bounds.origin.y}, block_sz};
    graphics_fill_rect(ctx, block, 0, GCornerNone);
  }
}

static void battery_copy_state(Facet* facet, BatteryChargeState charge) {
  facet->charge = charge;
  layer_mark_dirty(facet->layer);
}

static void vibrate_on_disconnect(Facet* facet, bool connected) {
  if (facet->last_bluetooth_state && !connected) {
    vibes_long_pulse();
  }
  facet->last_bluetooth_state = connected;
}

static void graphic_draw_layer(Layer *layer, GContext *ctx);
static void graphic_init_layer(Facet* facet);

static Facet facets[] = {
  {
    .init = &graphic_init_layer,
    .draw = &battery_draw_layer,
    .handle_battery = &battery_copy_state,
    .frame = {{0, 162}, {144, 3}},
  },
  {
    .handle_bluetooth = &vibrate_on_disconnect,
    .last_bluetooth_state = false,
  },
  {
    .init = &text_init_layer,
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_hours_to_words,
    .changes_on = HOUR_UNIT,
    .frame = {{0, -8}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_BOLD,
    .color = { .argb = GColorYellowARGB8 },
  },
  {
    .init = &text_init_layer,
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_minutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 32}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_LIGHT,
    .color = { .argb = GColorWhiteARGB8 },
  },
  {
    .init = &text_init_layer,
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_sminutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 72}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_LIGHT,
    .color = { .argb = GColorWhiteARGB8 },
  },
  {
    .init = &text_init_layer,
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_dates_to_words,
    .changes_on = DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
    .frame = {{0, 124}, {144, 36}},
    .font_key = FONT_KEY_GOTHIC_28_BOLD,
    .color = { .argb = GColorWhiteARGB8 },
  },
};

static int num_facets = sizeof(facets)/sizeof(facets[0]);

static void graphic_draw_layer(Layer *layer, GContext *ctx) {
  for (int i = 0; i < num_facets; i++) {
    Facet* facet = &(facets[i]);
    if (facet->layer == layer && facet->draw) {
      facet->draw(facet, layer, ctx);
      return;
    }
  }
}

static void graphic_init_layer(Facet* facet) {
  Layer *layer = facet->layer = layer_create(facet->frame);
  layer_set_update_proc(layer, &graphic_draw_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
 for (int i = 0; i < num_facets; i++) {
   Facet* facet = &(facets[i]);
   if (facet->handle_tick && (units_changed & facet->changes_on)) {
     facet->handle_tick(facet, tick_time, units_changed);
   }
 }
}

static void battery_handler(BatteryChargeState charge) {
  for (int i = 0; i < num_facets; i++) {
    Facet* facet = &(facets[i]);
    if (facet->handle_battery) {
      facet->handle_battery(facet, charge);
    }
  }
}

static void bluetooth_handler(bool connected) {
  for (int i = 0; i < num_facets; i++) {
    Facet* facet = &(facets[i]);
    if (facet->handle_bluetooth) {
      facet->handle_bluetooth(facet, connected);
    }
  }
}


static void main_window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  for (int i = 0; i < num_facets; i++) {
    Facet* facet = &(facets[i]);
    if (facet->init) {
      facet->init(facet);
    }
    if (facet->layer) {
      layer_add_child(window_get_root_layer(window), facet->layer);
    }
  }
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  tick_handler(tick_time, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT |
                          DAY_UNIT | MONTH_UNIT | YEAR_UNIT);
  battery_handler(battery_state_service_peek());
  bluetooth_handler(bluetooth_connection_service_peek());
}

static void main_window_unload(Window *window) { }

static void init(void) {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  bluetooth_connection_service_subscribe(bluetooth_handler);
}

static void deinit(void) {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

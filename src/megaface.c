#include <pebble.h>
#include "num2words.h"

static Window *s_main_window;

typedef struct _LayerInfo {
  Layer *layer;
  GRect frame;
  TimeUnits changes_on;
  GColor8 color;
  void (*handle_tick)(struct _LayerInfo* layer, struct tm *tick_time, TimeUnits units_changed);
  void (*handle_battery)(struct _LayerInfo* layer, BatteryChargeState charge);

  // Text layers
  TextLayer *textLayer;
  void (*get_text)(struct tm *t, char *buffer);
  char buffer[BUFFER_SIZE];
  const char* font_key;
} LayerInfo;

static void text_tick_handler(LayerInfo* layer, struct tm *tick_time, TimeUnits units_changed) {
  layer->get_text(tick_time, layer->buffer);
  text_layer_set_text(layer->textLayer, layer->buffer);
}

static void text_battery_handler(LayerInfo* layer, BatteryChargeState charge) {
  static char buffer[20];
  buffer[0] = charge.is_charging ? 'c':' ';
  buffer[1] = charge.is_plugged ? 'p' : ' ';
  buffer[2] = (charge.charge_percent / 100) + '0';
  buffer[3] = ((charge.charge_percent / 10) % 10) + '0';
  buffer[4] = (charge.charge_percent % 10) + '0';
  buffer[5] = 0;
  text_layer_set_text(layer->textLayer, buffer);
}

static LayerInfo layers[] = {
 { 
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_hours_to_words,
    .changes_on = HOUR_UNIT,
    .frame = {{0, -8}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_BOLD,
    .color = { .argb = GColorYellowARGB8 },
 },
 {  
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_minutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 32}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_LIGHT,
    .color = { .argb = GColorWhiteARGB8 },
  },
 { 
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_sminutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 72}, {144, 50}},
    .font_key = FONT_KEY_BITHAM_42_LIGHT,
    .color = { .argb = GColorWhiteARGB8 },
 },
 { 
    .handle_tick = &text_tick_handler,
    .get_text = &fuzzy_dates_to_words,
    .changes_on = DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
    .frame = {{0, 126}, {144, 36}},
    .font_key = FONT_KEY_GOTHIC_28_BOLD,
    .color = { .argb = GColorWhiteARGB8 },
 },
 { 
    .handle_battery = &text_battery_handler,
    .frame = {{100, 126}, {144, 36}},
    .font_key = FONT_KEY_GOTHIC_28_BOLD,
    .color = { .argb = GColorWhiteARGB8 },
 },
};

static int num_layers = sizeof(layers)/sizeof(layers[0]);

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
 for (int i = 0; i < num_layers; i++) {
   LayerInfo* layer = &(layers[i]);
   if (layer->handle_tick && (units_changed & layer->changes_on)) {
     layer->handle_tick(layer, tick_time, units_changed);
   }
 }
}

static void battery_handler(BatteryChargeState charge) {
 for (int i = 0; i < num_layers; i++) {
   LayerInfo* layer = &(layers[i]);
   if (layer->handle_battery) {
     layer->handle_battery(layer, charge);
   }
 }
}

static Layer* init_layer(LayerInfo* layer) {
  TextLayer *textLayer = layer->textLayer = text_layer_create(layer->frame);
  text_layer_set_background_color(textLayer, GColorClear);
  text_layer_set_text_alignment(textLayer, GTextAlignmentLeft);

  text_layer_set_text_color(textLayer, layer->color);
  text_layer_set_font(textLayer, fonts_get_system_font(layer->font_key));

  return text_layer_get_layer(textLayer);
}

static void main_window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  for (int i = 0; i < num_layers; i++) {
    layer_add_child(window_get_root_layer(window), init_layer(&layers[i]));
  }
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  tick_handler(tick_time, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT |
                          DAY_UNIT | MONTH_UNIT | YEAR_UNIT);
  battery_handler(battery_state_service_peek());
}

static void main_window_unload(Window *window) {

}

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

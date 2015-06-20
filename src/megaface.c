#include <pebble.h>
#include "num2words.h"

static Window *s_main_window;

typedef struct _LayerInfo {
  TextLayer *textLayer;
  Layer *layer;
  GRect frame;
  void (*get_text)(struct tm *t, char *buffer);
  TimeUnits changes_on;
  char buffer[BUFFER_SIZE];
} LayerInfo;

static LayerInfo layers[] = {
 { 
    .get_text = &fuzzy_hours_to_words,
    .changes_on = HOUR_UNIT,
    .frame = {{0, 0}, {144, 40}},
 },
 {  
    .get_text = &fuzzy_minutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 40}, {144, 40}},
  },
 { 
    .get_text = &fuzzy_sminutes_to_words,
    .changes_on = MINUTE_UNIT,
    .frame = {{0, 80}, {144, 40}},
 },
};
static int num_layers = sizeof(layers)/sizeof(layers[0]);

static void update_time(struct tm *tick_time, TimeUnits units_changed) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time START");
  for (int i = 0; i < num_layers; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time p1: %d", i);
    LayerInfo* layer = &(layers[i]);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time p2: l=%p", layer);
    if ((units_changed & layer->changes_on) == 0) {
      continue;
    }
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time p3: f=%p b=%p", layer->get_text, layer->buffer);
    layer->get_text(tick_time, layer->buffer);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time p4: %s", layer->buffer);
    text_layer_set_text(layer->textLayer, layer->buffer);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time p5");
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time, units_changed);
}

static Layer* init_layer(LayerInfo* layer) {
  TextLayer *textLayer = layer->textLayer = text_layer_create(layer->frame);
  text_layer_set_background_color(textLayer, GColorClear);
  text_layer_set_text_color(textLayer, GColorBlack);

  // Improve the layout to be more like a watchface
  text_layer_set_font(textLayer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  return text_layer_get_layer(textLayer);
}

static void main_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing on window: %p", window);
  for (int i = 0; i < num_layers; i++) {
    layer_add_child(window_get_root_layer(window), init_layer(&layers[i]));
  }
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  update_time(tick_time, SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT);
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

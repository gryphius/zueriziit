#include "fuzzy.h"
#include <pebble.h>
  
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_shadow_layer;
static Layer *s_canvas_layer;
static GPath *s_hour_hand;

static GColor8 s_main_colour;
static GColor8 s_high_colour;
static GColor8 s_dark_colour;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
  WEATHER_TEMPERATURE_KEY = 0x3  // TUPLE_CSTRING
};

static int increment_hour(int hour) {
  hour += 1;
  if (hour == 13){
    hour = 1;
  }
  return hour;
}

static void set_colour(uint8_t temp){
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting temp: %d", temp);
  if(temp > 100){
    //error, or apocalypse. Either way, black.
    s_main_colour = GColorDarkGray;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorBlack;
  }else if(temp > 25){
    //red
    s_main_colour = GColorSunsetOrange;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorRed;
  }else if(temp > 20){
    //dark orange?
    s_main_colour = GColorChromeYellow;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorOrange;
  }else if(temp > 15){
    //orange
    s_main_colour = GColorRajah;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorOrange;
  }else if(temp > 10){
    //green
    s_main_colour = GColorScreaminGreen;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorKellyGreen;
  }else if(temp > 5){
    //blue
    s_main_colour = GColorBlueMoon;
    s_high_colour = GColorCeleste;
    s_dark_colour = GColorOxfordBlue;
  }else if(temp > 0){
    //blue
    s_main_colour = GColorVeryLightBlue;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorOxfordBlue;
  }else{
    //white
    s_main_colour = GColorPictonBlue;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorOxfordBlue;
  }
  window_set_background_color(s_main_window,s_main_colour);
  text_layer_set_text_color(s_time_layer, s_high_colour);
  text_layer_set_text_color(s_shadow_layer, s_dark_colour);
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      set_colour(new_tuple->value->uint8);
      break;
  }
}

static void request_weather(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Error creating outbound message");
    return;
  }

  int value = 1;
  dict_write_int(iter, 1, &value, sizeof(int), true);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void update_graphics(Layer *lyr, GContext *ctx) {
  GRect bounds = layer_get_bounds(s_canvas_layer);
  graphics_context_set_antialiased(ctx, true);

  // Get the center of the screen (non full-screen)
  GPoint center = GPoint(bounds.size.w / 2, (bounds.size.h / 2));
  GPoint shadow = GPoint(center.x + 2, center.y + 2);

  // Draw the circle
  graphics_context_set_stroke_width(ctx,5);
  
  graphics_context_set_stroke_color(ctx, s_dark_colour);
  graphics_draw_circle(ctx, shadow, 35);
  
  graphics_context_set_stroke_color(ctx, s_high_colour);
  graphics_draw_circle(ctx, center, 35);

  // Draw the hand
  time_t temp = time(NULL); 
  struct tm *t = localtime(&temp);
  
  gpath_move_to(s_hour_hand, shadow);
  graphics_context_set_fill_color(ctx, s_dark_colour);
  gpath_rotate_to(s_hour_hand, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_hand);
  
  gpath_move_to(s_hour_hand, center);
  graphics_context_set_fill_color(ctx, s_high_colour);
  gpath_rotate_to(s_hour_hand, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_hand);
}

static char *get_hour(int h){
  if(h > 12){
    h -= 12;
  }
  return (char *)HOURS[h-1];
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  if(true || tick_time->tm_min % 30 == 0){
    // request weather every 30 minutes
    request_weather();
  }

  // Create a long-lived buffer
  static char buffer[] = "nearly quarter past eleven";
//   text_layer_set_text(s_time_layer, buffer);
//   return;
  
  int minutes = tick_time->tm_min;
  char *hour = get_hour(tick_time->tm_hour);
  char *next_hour = get_hour(increment_hour(tick_time->tm_hour));
  if(minutes <= 5 || minutes > 55){
    // ish
    if(minutes > 10){
      snprintf(buffer, sizeof("eleven-ish"), "%s-ish", next_hour);
    }else{
      snprintf(buffer, sizeof("eleven-ish"), "%s-ish", hour);
    }
  }else if(minutes > 5 && minutes <= 10){
    // past
    snprintf(buffer, sizeof("Past eleven"), "past %s", hour);
  }else if(minutes > 10 && minutes <= 20){
    // quarter past
    snprintf(buffer, sizeof("Quarter past eleven"), "quarter past %s", hour);
  }else if(minutes > 20 && minutes <= 25){
    // nearly half past
    snprintf(buffer, sizeof("Nearly half past eleven"), "nearly half past %s", hour);
  }else if(minutes > 25 && minutes <= 35){
    // half past
    snprintf(buffer, sizeof("Half past eleven"), "half past %s", hour);
  }else if(minutes > 35 && minutes <= 40){
    // nearly quarter to
    snprintf(buffer, sizeof("Nearly quarter to eleven"), "nearly quarter to %s", next_hour);
  }else if(minutes > 40 && minutes <= 50){
    // quarter to
    snprintf(buffer, sizeof("Quarter to eleven"), "quarter to %s", next_hour);
  }else if(minutes > 50 && minutes <= 55){
    // nearly
    snprintf(buffer, sizeof("Nearly eleven"), "nearly %s", next_hour);
  }
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
  text_layer_set_text(s_shadow_layer, buffer);
}

static void main_window_load(Window *window) {
  
  s_main_colour = GColorBlack;
  s_high_colour = GColorWhite;
  s_dark_colour = GColorDarkGray;
  
  s_hour_hand = gpath_create(&HOUR_HAND_POINTS);
  
  window_set_background_color(window, s_main_colour);
  
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(5, 90, 135, 75));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_high_colour);
  text_layer_set_text(s_time_layer, "...");

  s_shadow_layer = text_layer_create(GRect(7, 92, 135, 75));
  text_layer_set_background_color(s_shadow_layer, GColorClear);
  text_layer_set_text_color(s_shadow_layer, s_dark_colour);
  text_layer_set_text(s_shadow_layer, "...");
  
  // Improve the layout to be more like a watchface
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_time_layer, GTextOverflowModeWordWrap);
  
  text_layer_set_font(s_shadow_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_shadow_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_shadow_layer, GTextOverflowModeWordWrap);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_shadow_layer));
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  // Create a Graphics Layer and set the update_proc
  s_canvas_layer = layer_create(GRect(0, 0, 144, 100));
  layer_set_update_proc(s_canvas_layer, update_graphics);
  
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // Make sure the time is displayed from the start
  update_time();
  
  Tuplet initial_values[] = {
    TupletCString(WEATHER_TEMPERATURE_KEY, "0")
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL
  );

  request_weather();
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_shadow_layer);
  layer_destroy(s_canvas_layer);
  gpath_destroy(s_hour_hand);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}
  
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  app_message_open(64, 64);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
  app_sync_deinit(&s_sync);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

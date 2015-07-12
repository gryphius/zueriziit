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

static uint8_t s_charge;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];




static void update_graphics(Layer *lyr, GContext *ctx) {
  GRect bounds = layer_get_bounds(s_canvas_layer);
  graphics_context_set_antialiased(ctx, true);

  // Get the center of the screen (non full-screen)
  GPoint center = GPoint(bounds.size.w / 2, (bounds.size.h / 2) + 4);
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
  
  // Draw the battery level
  graphics_fill_rect(ctx, GRect(0,0,((bounds.size.w * s_charge) / 100) + 2,7),0,GCornerNone);
  
  gpath_move_to(s_hour_hand, center);
  graphics_context_set_fill_color(ctx, s_high_colour);
  gpath_rotate_to(s_hour_hand, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_hand);
  
  // Draw the battery level
  graphics_fill_rect(ctx, GRect(0,0,(bounds.size.w * s_charge) / 100,5),0,GCornerNone);
  
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  int minutes = tick_time->tm_min;
  int hours = tick_time->tm_hour % 12;

  // Create a long-lived buffer
  static char buffer[] = "foif vor halbi sächsi";
//   text_layer_set_text(s_time_layer, buffer);
//   return;
  
  char *hour = (char *)HOURS[hours];
  hours = (hours + 1) % 12;
  char *next_hour = (char *)HOURS[hours];
  if(minutes <= 3 || minutes > 57){
    // ish
    if(minutes > 10){
      snprintf(buffer, sizeof("Zwölfi"), "%s", next_hour);
    }else{
      snprintf(buffer, sizeof("Zwölfi"), "%s", hour);
    }
  }else if(minutes > 3 && minutes <= 7){
    snprintf(buffer, sizeof("Foif ab Zwölfi"), "Foif ab %s", hour);
  }else if(minutes > 7 && minutes <= 12){
    snprintf(buffer, sizeof("Zäh ab Zwölfi"), "Zä ab %s", hour);
  }else if(minutes > 12 && minutes <= 17){
    snprintf(buffer, sizeof("Viertel ab zwölfi"), "Viertel ab %s", hour);
  }else if(minutes > 17 && minutes <= 22){
    snprintf(buffer, sizeof("Zwänzg ab zwölfi"), "Zwänzg ab %s", hour);
  }else if(minutes > 22 && minutes <= 27){
    snprintf(buffer, sizeof("Foif vor Halbi Zwölfi"), "Foif vor halbi %s", next_hour);
  }else if(minutes > 27 && minutes <= 32){
    snprintf(buffer, sizeof("Halbi zwölfi"), "halbi %s", next_hour);
  }else if(minutes > 32 && minutes <= 37){
    snprintf(buffer, sizeof("Foif ab halbi zwölfi"), "Foif ab halbi %s", next_hour);
  }else if(minutes > 37 && minutes <= 42){
    snprintf(buffer, sizeof("Zwänzg vor zwölfi"), "Zwänzg vor %s", next_hour);
  }else if(minutes > 42 && minutes <= 47){
    snprintf(buffer, sizeof("Viertel vor zwölfi"), "Viertel vor %s", next_hour);
  }else if(minutes > 47 && minutes <= 52){
    snprintf(buffer, sizeof("Zäh vor zwölfi"), "Zä vor %s", next_hour);
  }
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
  text_layer_set_text(s_shadow_layer, buffer);
}

static void main_window_load(Window *window) {
  
   s_main_colour = GColorVeryLightBlue;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorOxfordBlue;
  
  s_hour_hand = gpath_create(&HOUR_HAND_POINTS);
  
  window_set_background_color(window, s_main_colour);
  
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(5, 95, 135, 70));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, s_high_colour);
  text_layer_set_text(s_time_layer, "...");

  s_shadow_layer = text_layer_create(GRect(7, 97, 135, 70));
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
  
  // initialise battery display
  
  BatteryChargeState charge = battery_state_service_peek();
  s_charge = charge.charge_percent;
  
  // Make sure the time is displayed from the start
  update_time();
  
  /*
  Tuplet initial_values[] = {
    TupletCString(WEATHER_TEMPERATURE_KEY, "0")
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL
  );

  request_weather();
  */
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_shadow_layer);
  layer_destroy(s_canvas_layer);
  gpath_destroy(s_hour_hand);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void battery_handler(BatteryChargeState charge) {
  s_charge = charge.charge_percent;
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
  battery_state_service_subscribe(battery_handler);
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

#include <pebble.h>
#include <inttypes.h>

// for development purpose only!
#define DEBUG_LAYERS 0
#define DEBUG_DIGITS 0
#define DEBUG_BACKGROUND_SWITCH 0
#define DEBUG_TIMER 0
#define DEBUG_CONFIG 0

#define TEXT_COLOR GColorBlack
#define STRAIGHT_SMALL_FONT 1

#define MAX(x, y) ((x) > (y) ? (x) : (y))

static Window *s_main_window;

static GFont s_big_font;
static GFont s_small_font;

static TextLayer *s_hours_layer;
static TextLayer *s_minutes_layer;
static TextLayer *s_seconds_layer;
static TextLayer *s_battery_layer;

static GBitmap *s_bt_bitmap;
static GBitmap *s_background_bitmap;

static BitmapLayer *s_bt_bitmap_layer;
static BitmapLayer *s_background_bitmap_layer;

#ifdef PBL_HEALTH
static GBitmap *s_steps_bitmap;
static TextLayer *s_steps_layer;
static BitmapLayer *s_steps_bitmap_layer;
#endif

static int backgrounds[] = {
  RESOURCE_ID_VADER,
  RESOURCE_ID_LUKE,
  RESOURCE_ID_SOLO,
  RESOURCE_ID_LUKE2,
  RESOURCE_ID_C3PO,
  RESOURCE_ID_REY,
  RESOURCE_ID_FINN,
  RESOURCE_ID_REY2,
};
static uint8_t current_background_index = 0;

enum switch_event {
  SWITCH_MINUTE,
  SWITCH_TAP,
  SWITCH_TIMER,
  SWITCH_ENUM_ITEMS
};
static AppTimer * switch_app_timer = NULL;

enum third_line_options {
  TL_NOTHING,
  TL_SECONDS,
  TL_DATE
};

// settings
static enum third_line_options third_line = TL_NOTHING;
static bool steps = 1;
static bool characters[] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static bool switch_events[] = { 1, 1, 0 };
static uint8_t switch_timer = 1;
static uint8_t bluetoothvibe = 0;


static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "+100%";

#if ! DEBUG_DIGITS
  snprintf(battery_text, sizeof(battery_text), "%s%d%%",
           (charge_state.is_charging ? "+" : ""),
           charge_state.charge_percent);
#endif
  text_layer_set_text(s_battery_layer, battery_text);
}

static void handle_bluetooth(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(s_bt_bitmap_layer), !connected);
  
  // vibrate on connection loss
  if (bluetoothvibe && !connected) {
    uint32_t segments[] = { bluetoothvibe };
    VibePattern pat = { segments, ARRAY_LENGTH(segments) };
    vibes_enqueue_custom_pattern(pat);
  }
}

#ifdef PBL_HEALTH
static void handle_health(HealthEventType event, void *context) {
  static char steps_text[] = "00000";

  if (event == HealthEventMovementUpdate) {
#if ! DEBUG_DIGITS
    snprintf(steps_text, sizeof(steps_text),
             "%d", (int)health_service_sum_today(HealthMetricStepCount));
#endif
    text_layer_set_text(s_steps_layer, steps_text);
  }
}
#endif

static void switch_background() {
#if DEBUG_BACKGROUND_SWITCH
  APP_LOG(APP_LOG_LEVEL_DEBUG, "switch_background called");
#endif

  uint8_t start = current_background_index; // save start character index
  do {
      current_background_index++; // select next character
      if (current_background_index == ARRAY_LENGTH(backgrounds)) // we have reached the array end, starting over
          current_background_index = 0;
      if (current_background_index == start) // are we back at the starting position?
          return;
  } while (! characters[current_background_index]); // until the current character is enabled

#if DEBUG_BACKGROUND_SWITCH
  APP_LOG(APP_LOG_LEVEL_DEBUG, "New background index: %u", current_background_index);
#endif

  // switch image
  gbitmap_destroy(s_background_bitmap);
  s_background_bitmap = gbitmap_create_with_resource( backgrounds[ current_background_index ] );
  bitmap_layer_set_bitmap(s_background_bitmap_layer, s_background_bitmap);
}

static void handle_tap(AccelAxisType axis, int32_t direction) {
  // TODO: optimize switch tap: (un)subscribe tap listener!
  if (switch_events[SWITCH_TAP])
      switch_background();
}

static void handle_tick(struct tm* tick_time, TimeUnits units_changed) {
    static char s_hours_text[] = "00";
    static char s_minutes_text[] = "00";
    static char s_seconds_text[] = "00-00";

#if ! DEBUG_DIGITS
    strftime(s_hours_text,   sizeof(s_hours_text),   "%H", tick_time);
    strftime(s_minutes_text, sizeof(s_minutes_text), "%M", tick_time);
#endif
    text_layer_set_text(s_hours_layer,   s_hours_text);
    text_layer_set_text(s_minutes_layer, s_minutes_text);
  
#if ! DEBUG_DIGITS
    if (third_line == TL_SECONDS)
        snprintf(s_seconds_text, sizeof(s_seconds_text),
                 "%d %d", tick_time->tm_sec / 10, tick_time->tm_sec % 10);
    else if (third_line == TL_DATE)
        snprintf(s_seconds_text, sizeof(s_seconds_text),
                 "%02d-%02d", tick_time->tm_mon + 1, tick_time->tm_mday);
    else s_seconds_text[0] = '\0';
#endif
    text_layer_set_text(s_seconds_layer, s_seconds_text);

    if (switch_events[SWITCH_MINUTE] && tick_time->tm_sec == 0) { // switch every minute if needed
        switch_background();
    }
}

static void run_handle_tick() {
    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    handle_tick(current_time, SECOND_UNIT);
}

static void switch_timer_destroy()
{
    if (switch_app_timer != NULL)
    {
#if DEBUG_TIMER
        APP_LOG(APP_LOG_LEVEL_INFO, "Switch character timer destroyed");
#endif
        app_timer_cancel(switch_app_timer);
        switch_app_timer = NULL;
    }
}

static void switch_timer_reinit(); // declaration

static void handle_switch_timer(void *data)
{
    switch_app_timer = NULL; // timer was fired, to we have to clear it
#if DEBUG_TIMER
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Switch character timer called");
#endif
    switch_background();
    switch_timer_reinit();
}

static void switch_timer_reinit()
{
    // first: destroy previous
    switch_timer_destroy();

    if (switch_events[SWITCH_TIMER])
    {
#if DEBUG_TIMER
        APP_LOG(APP_LOG_LEVEL_INFO, "Switch character timer registred: %d", switch_timer);
#endif
        switch_app_timer = app_timer_register((uint32_t)switch_timer * 1000, handle_switch_timer, NULL);
    }
}

#ifdef PBL_HEALTH
static void update_steps_visibility()
{
  // TODO: optimize (un)subscribe!
  layer_set_hidden(text_layer_get_layer(s_steps_layer), !steps);
  layer_set_hidden(bitmap_layer_get_layer(s_steps_bitmap_layer), !steps);
}
#endif

#define PERSIST_DATA(operation, structure) \
{ \
  int res = persist_ ## operation ## _data(MESSAGE_KEY_ ## structure, structure, sizeof(structure)); \
  if (res != sizeof(structure)) APP_LOG(APP_LOG_LEVEL_ERROR, #structure " persist " #operation " error: %d", res); \
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
#if DEBUG_CONFIG
  APP_LOG(APP_LOG_LEVEL_INFO, "App message received. Minimum size: %u bytes", dict_size(iterator));
#endif

  for (Tuple * item = dict_read_first(iterator);
       item != NULL;
       item = dict_read_next(iterator))
  {
      if (item->key == MESSAGE_KEY_third_line) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Third line: %s", item->value->cstring);
#endif
          switch (item->value->cstring[0])
          {
            case 'n': third_line = TL_NOTHING; break;
            case 's': third_line = TL_SECONDS; break;
            case 'd': third_line = TL_DATE;    break;
            default: APP_LOG(APP_LOG_LEVEL_ERROR, "Third line: '%s'", item->value->cstring);
          }
          persist_write_int(MESSAGE_KEY_third_line, third_line);

          text_layer_set_text(s_seconds_layer, "");

          // tick service (and seconds) upgrade
          tick_timer_service_unsubscribe();
          tick_timer_service_subscribe(third_line == TL_SECONDS ? SECOND_UNIT : MINUTE_UNIT, handle_tick);
      }
      else if (item->key == MESSAGE_KEY_steps) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Show steps: %d", item->value->uint16);
#endif
          steps = (item->value->uint16 > 0);
          persist_write_int(MESSAGE_KEY_steps, steps);
#ifdef PBL_HEALTH
          update_steps_visibility();
#endif
      }
      else if (item->key >= MESSAGE_KEY_characters &&
               item->key < (MESSAGE_KEY_characters + ARRAY_LENGTH(backgrounds))) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Character: %d = %d",
                  item->key - MESSAGE_KEY_characters,
                  item->value->uint16);
#endif
          int index = (item->key - MESSAGE_KEY_characters); // get the current character index
          characters[index] = (item->value->uint16 > 0); // check if enabled or not

          // write to persistent storage later, not at every single element
      }
      else if (item->key >= MESSAGE_KEY_switch_events &&
               item->key < (MESSAGE_KEY_switch_events + SWITCH_ENUM_ITEMS)) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Switch event: %d = %d",
                  item->key - MESSAGE_KEY_switch_events,
                  item->value->uint16);
#endif
          int index = (item->key - MESSAGE_KEY_switch_events); // get the current switch events index
          switch_events[index] = (item->value->uint16 > 0); // check if enabled or not

          // write to persistent storage and switch timer reinit called later, not at every single element
      }
      else if (item->key == MESSAGE_KEY_switch_timer) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Switch timer: %d", item->value->uint8);
#endif
          // validate value: between 1 and 59
          if (item->value->uint8 >=  1 &&
              item->value->uint8 <= 59) {
              switch_timer = item->value->uint8;
              persist_write_int(MESSAGE_KEY_switch_timer, switch_timer);
          }
          else
              APP_LOG(APP_LOG_LEVEL_ERROR, "Switch timer value is invalid: %d", item->value->uint8);
      }
      else if (item->key == MESSAGE_KEY_bluetoothvibe) {
#if DEBUG_CONFIG
          APP_LOG(APP_LOG_LEVEL_INFO, "Bluetoothvibe: %d", item->value->uint16);
#endif
          // validate value: between 0 and 1000ms, and multiply of 50ms
          if ( item->value->uint16 <= 100 && // unsigned wont be less than 0
              (item->value->uint16 %    5 == 0)) {
              bluetoothvibe = item->value->uint16 * 10; // convert to miliseconds instead of hundredth of a second
              persist_write_int(MESSAGE_KEY_bluetoothvibe, bluetoothvibe);
          }
          else
              APP_LOG(APP_LOG_LEVEL_ERROR, "Bluetoothvibe value is invalid: %d", item->value->uint16);
      }
      else {
          APP_LOG(APP_LOG_LEVEL_ERROR, "Unhandled Touple item!");
          APP_LOG(APP_LOG_LEVEL_ERROR, "   key: %" PRId32, item->key);
          APP_LOG(APP_LOG_LEVEL_ERROR, "  type: %d", item->type);
          APP_LOG(APP_LOG_LEVEL_ERROR, "length: %d", item->length);
      }
  }
  
  // write full arrays once to persistent storage
  PERSIST_DATA(write, characters);
  PERSIST_DATA(write, switch_events);

  // start/stop timer if needed
  switch_timer_reinit();

  run_handle_tick(); // refresh display after all options are processed
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped. Reason: %d", (int)reason);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  
  // --- Load settings
  
#define GET_PERSIST_VALUE_OR_DEFAULT(key, def) \
  (persist_exists( key ) ? persist_read_int( key ) : def)

  third_line = GET_PERSIST_VALUE_OR_DEFAULT(MESSAGE_KEY_third_line, TL_NOTHING);
  steps = GET_PERSIST_VALUE_OR_DEFAULT(MESSAGE_KEY_steps, 1);
  switch_timer = GET_PERSIST_VALUE_OR_DEFAULT(MESSAGE_KEY_switch_timer, 1);
  bluetoothvibe = GET_PERSIST_VALUE_OR_DEFAULT(MESSAGE_KEY_bluetoothvibe, 0);

  if (persist_exists(MESSAGE_KEY_characters))
      PERSIST_DATA(read, characters)
  // else: initialization is done at declaration

  if (persist_exists(MESSAGE_KEY_switch_events))
      PERSIST_DATA(read, switch_events)
  // else: initialization is done at declaration

  // --- Fonts

  s_big_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DOTTED_76));

#if STRAIGHT_SMALL_FONT
  s_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_24);
#define BOX_H 24
#else
  // hard to read
  s_small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DOTTED_25));
#define BOX_H 26
#endif

  // --- Text layers init

#define TOP_SPACE -30
#define LEFT_SPACE 0

  // hours
  s_hours_layer= text_layer_create(GRect(LEFT_SPACE, TOP_SPACE, 80, 80));
  text_layer_set_text_alignment(s_hours_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_hours_layer, TEXT_COLOR);
  text_layer_set_background_color(s_hours_layer, DEBUG_LAYERS ? GColorRed : GColorClear);
  text_layer_set_font(s_hours_layer, s_big_font);

  // minutes
  s_minutes_layer= text_layer_create(GRect(LEFT_SPACE, TOP_SPACE + 48, 80, 80));
  text_layer_set_text_alignment(s_minutes_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_minutes_layer, TEXT_COLOR);
  text_layer_set_background_color(s_minutes_layer, DEBUG_LAYERS ? GColorBlue : GColorClear);
  text_layer_set_font(s_minutes_layer, s_big_font);

  // seconds
  s_seconds_layer = text_layer_create(GRect(2, 90, bounds.size.w / 2, 30));
  text_layer_set_text_alignment(s_seconds_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_seconds_layer, TEXT_COLOR);
  text_layer_set_text(s_seconds_layer, "");
  text_layer_set_background_color(s_seconds_layer, DEBUG_LAYERS ? GColorRed : GColorClear);
  text_layer_set_font(s_seconds_layer, s_small_font);

#undef  LEFT_SPACE
#define LEFT_SPACE 5
#define BT_W 7
#define BT_H 15
#define STEPS_W_H 16

#ifdef PBL_HEALTH
  // steps
  s_steps_layer = text_layer_create(GRect(LEFT_SPACE + BT_W + LEFT_SPACE, bounds.size.h - (BOX_H * 2),
                                          bounds.size.w / 2 - 10 /* fix offset */, BOX_H));
  text_layer_set_text_color(s_steps_layer, TEXT_COLOR);
  text_layer_set_background_color(s_steps_layer, DEBUG_LAYERS ? GColorRed : GColorClear);
  text_layer_set_font(s_steps_layer, s_small_font);
#endif

  // battery
  s_battery_layer = text_layer_create(GRect(LEFT_SPACE + BT_W + LEFT_SPACE, bounds.size.h - BOX_H - 5 /* fix offset */,
                                            bounds.size.w / 2 - 10 /* fix offset */, BOX_H));
  text_layer_set_text_color(s_battery_layer, TEXT_COLOR);
  text_layer_set_background_color(s_battery_layer, DEBUG_LAYERS ? GColorRed : GColorClear);
  //text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_font(s_battery_layer, s_small_font);
  
  // --- Bitmap layers init
  
#ifdef PBL_HEALTH
  // steps icon
  s_steps_bitmap = gbitmap_create_with_resource(RESOURCE_ID_STEPS);
  s_steps_bitmap_layer = bitmap_layer_create(GRect(0, bounds.size.h - BOX_H - STEPS_W_H + 1 /* fix offset */, STEPS_W_H, STEPS_W_H));
  bitmap_layer_set_bitmap(s_steps_bitmap_layer, s_steps_bitmap);
  bitmap_layer_set_compositing_mode(s_steps_bitmap_layer, GCompOpSet);
#endif

  // BT icon
  s_bt_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BT);
  s_bt_bitmap_layer = bitmap_layer_create(GRect(LEFT_SPACE, bounds.size.h - BOX_H + (BOX_H - BT_H) / 2, BT_W, BT_H));
  bitmap_layer_set_bitmap(s_bt_bitmap_layer, s_bt_bitmap);
  bitmap_layer_set_compositing_mode(s_bt_bitmap_layer, GCompOpSet);

  // background
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_VADER);
  s_background_bitmap_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_background_bitmap_layer, s_background_bitmap);

  // --- Layers registration

  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_bitmap_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bt_bitmap_layer));

  layer_add_child(window_layer, text_layer_get_layer(s_hours_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_minutes_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_seconds_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));

#ifdef PBL_HEALTH
  layer_add_child(window_layer, bitmap_layer_get_layer(s_steps_bitmap_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_steps_layer));

  update_steps_visibility();
#endif

  // --- Time handler

  run_handle_tick();

  tick_timer_service_subscribe(third_line == TL_SECONDS ? SECOND_UNIT : MINUTE_UNIT, handle_tick);

  switch_timer_reinit();

  // --- Battery handler

  battery_state_service_subscribe(handle_battery);

  handle_battery(battery_state_service_peek());

  // --- Connection handler

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });

  handle_bluetooth(connection_service_peek_pebble_app_connection());
  
  // --- Health handler

#ifdef PBL_HEALTH
  if (health_service_events_subscribe(handle_health, NULL))
    handle_health(HealthEventMovementUpdate, NULL);
  else
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
#endif

  // --- TAP handler
  
  accel_tap_service_subscribe(handle_tap);
  
  // --- App message init

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);

  app_message_open(MAX(APP_MESSAGE_INBOX_SIZE_MINIMUM, 512), 0);
}

static void main_window_unload(Window *window) {
  // --- Handlers 

  tick_timer_service_unsubscribe();
  
  switch_timer_destroy();
  
  battery_state_service_unsubscribe();
  
  connection_service_unsubscribe();

#ifdef PBL_HEALTH
  health_service_events_unsubscribe();
#endif

  accel_tap_service_unsubscribe();
  
  app_message_deregister_callbacks();
  
  // --- Bitmap layers
  
  gbitmap_destroy(s_bt_bitmap);
  gbitmap_destroy(s_background_bitmap);

  bitmap_layer_destroy(s_bt_bitmap_layer);
  bitmap_layer_destroy(s_background_bitmap_layer);

#ifdef PBL_HEALTH
  gbitmap_destroy(s_steps_bitmap);
  bitmap_layer_destroy(s_steps_bitmap_layer);
#endif

  // --- Text layers
  
  text_layer_destroy(s_hours_layer);
  text_layer_destroy(s_minutes_layer);
  text_layer_destroy(s_seconds_layer);  
  text_layer_destroy(s_battery_layer);

#ifdef PBL_HEALTH
  text_layer_destroy(s_steps_layer);
#endif

  // --- Fonts

  fonts_unload_custom_font(s_big_font);

#if !STRAIGHT_SMALL_FONT
  fonts_unload_custom_font(s_small_font);
#endif
}

static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

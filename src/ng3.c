/*
Copyright (C) 2014 Mark Reed

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "pebble.h"

GColor background_color = GColorBlack;
GColor text_color = GColorWhite;

static AppSync sync;
static uint8_t sync_buffer[64];

static int blink;
static int invert;
static int bluetoothvibe;
static int hourlyvibe;

static bool appStarted = false;

enum {
  BLINK_KEY = 0x0,
  INVERT_KEY = 0x1,
  BLUETOOTHVIBE_KEY = 0x2,
  HOURLYVIBE_KEY = 0x3
};

Window *window;
TextLayer *layer_date_text;
TextLayer *layer_time_text;

TextLayer *layer_week_text;

static GFont *time_font;
static GFont *date_font;

int cur_day = -1;
int charge_percent = 0;

TextLayer *battery_text_layer;
InverterLayer *inverter_layer = NULL;



void change_background(bool invert) {
  if (invert && inverter_layer == NULL) {
    // Add inverter layer
    Layer *window_layer = window_get_root_layer(window);

    inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));

  } else if (!invert && inverter_layer != NULL) {
    // Remove Inverter layer
    layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
    inverter_layer_destroy(inverter_layer);
    inverter_layer = NULL;
  }
  // No action required
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case BLINK_KEY:
      blink = new_tuple->value->uint8;
      tick_timer_service_unsubscribe();
      if(blink) {
        tick_timer_service_subscribe(SECOND_UNIT, handle_minute_tick);
      }
      else {
        tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
      }
	  
      break;
	  
    case INVERT_KEY:
      invert = new_tuple->value->uint8;
      change_background(invert);
      break;
    
	case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8;
      break;      
     
	case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8;
      break;
  }
}
void update_battery_state(BatteryChargeState charge_state) {
    static char battery_text[] = "x100%";

    if (charge_state.is_charging) {

        snprintf(battery_text, sizeof(battery_text), "+%d%%", charge_state.charge_percent);
    } else {
        snprintf(battery_text, sizeof(battery_text), "%d%%", charge_state.charge_percent);
        
    }
    charge_percent = charge_state.charge_percent;
    
    text_layer_set_text(battery_text_layer, battery_text);
	
	
	if(charge_percent >= 50) {
      layer_set_hidden(text_layer_get_layer(battery_text_layer), true);
	}
	    if (charge_state.is_charging) {
        layer_set_hidden(text_layer_get_layer(battery_text_layer), false);
		}      	
} 

static void toggle_bluetooth(bool connected) {
  if(appStarted && !connected && bluetoothvibe) {
    //vibe!
    vibes_long_pulse();
  }
//  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), !connected);
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth(connected);
}

void update_time(struct tm *tick_time) {

	static char time_text[] = "00:00";
    static char date_text[] = "xxx xxx 00xx xx xxx";
	static char week_text[] = "xxxx 00, 0000";
    
    char *time_format;

    int new_cur_day = tick_time->tm_year*1000 + tick_time->tm_yday;
    if (new_cur_day != cur_day) {
        cur_day = new_cur_day;

			switch(tick_time->tm_mday)
  {
    case 1 :
    case 21 :
    case 31 :
      strftime(date_text, sizeof(date_text), "%a, %est of %b", tick_time);
      break;
    case 2 :
    case 22 :
      strftime(date_text, sizeof(date_text), "%a, %end of %b", tick_time);
      break;
    case 3 :
    case 23 :
      strftime(date_text, sizeof(date_text), "%a, %erd of %b", tick_time);
      break;
    default :
      strftime(date_text, sizeof(date_text), "%a, %eth of %b", tick_time);
      break;
  }
	
	  text_layer_set_text(layer_date_text, date_text);
		
	    strftime(week_text, sizeof(week_text), "Week %V, %Y", tick_time);
        text_layer_set_text(layer_week_text, week_text);
		
    }

    if (clock_is_24h_style()) {
        time_format = "%R";
		
    } else {
        time_format = "%I:%M";
		
    }

    strftime(time_text, sizeof(time_text), time_format, tick_time);

    if (!clock_is_24h_style() && (time_text[0] == '0')) {
        memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }

    text_layer_set_text(layer_time_text, time_text);


	
}

void set_style(void) {
    
    background_color  = GColorBlack;
    text_color = GColorWhite;
	
	// set-up layer colours
    window_set_background_color(window, background_color);
    text_layer_set_text_color(layer_time_text, text_color);
    text_layer_set_text_color(layer_date_text, text_color);
    text_layer_set_text_color(battery_text_layer, text_color);
    text_layer_set_text_color(layer_week_text, text_color);
}

void force_update(void) {
    toggle_bluetooth(bluetooth_connection_service_peek());
    time_t now = time(NULL);
    update_time(localtime(&now));
}

void hourvibe (struct tm *tick_time) {

	  if(appStarted && hourlyvibe) {
    //vibe!
    vibes_short_pulse();
  }
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    update_time(tick_time);
if (units_changed & HOUR_UNIT) {
    hourvibe(tick_time);
  }}


void handle_init(void) {
	
	const int inbound_size = 64;
    const int outbound_size = 64;
    app_message_open(inbound_size, outbound_size);  

	
    window = window_create();
    window_stack_push(window, true);
 
	// resources
	time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FREESANS_46));
    date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FREESANS_14));
	
    // layer position and alignment
    layer_time_text = text_layer_create(GRect(0, 57, 144, 50));
    layer_date_text = text_layer_create(GRect(0, 103, 144, 20));
    battery_text_layer = text_layer_create(GRect(0, 3, 144, 22));
    layer_week_text = text_layer_create(GRect(0, 52, 144, 22));


    text_layer_set_background_color(layer_date_text, GColorClear);
    text_layer_set_font(layer_date_text, date_font);
    text_layer_set_background_color(layer_time_text, GColorClear);
    text_layer_set_font(layer_time_text, time_font);
    text_layer_set_background_color(battery_text_layer, GColorClear);
    text_layer_set_font(battery_text_layer, date_font);
	text_layer_set_background_color(layer_week_text, GColorClear);
    text_layer_set_font(layer_week_text, date_font);

	
    text_layer_set_text_alignment(layer_date_text, GTextAlignmentCenter);
    text_layer_set_text_alignment(layer_time_text, GTextAlignmentCenter);
    text_layer_set_text_alignment(battery_text_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(layer_week_text, GTextAlignmentCenter);

    // composing layers
    Layer *window_layer = window_get_root_layer(window);

    layer_add_child(window_layer, text_layer_get_layer(layer_date_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_time_text));
    layer_add_child(window_layer, text_layer_get_layer(battery_text_layer));
    layer_add_child(window_layer, text_layer_get_layer(layer_week_text));

    set_style();
	
    // handlers
    battery_state_service_subscribe(update_battery_state);
    bluetooth_connection_service_subscribe(&toggle_bluetooth);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	
	Tuplet initial_values[] = {
    TupletInteger(BLINK_KEY, 1),
    TupletInteger(BLUETOOTHVIBE_KEY, 0),
    TupletInteger(HOURLYVIBE_KEY, 0),
    TupletInteger(INVERT_KEY, 0)
  };
  
    app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, NULL, NULL);
   
    appStarted = true;
	
	
	// update the battery on launch
    update_battery_state(battery_state_service_peek());
	
    // draw first frame
    force_update();
}

void handle_deinit(void) {
  app_sync_deinit(&sync);

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
	
  text_layer_destroy( layer_time_text );
  text_layer_destroy( layer_date_text );
  text_layer_destroy( battery_text_layer );
  text_layer_destroy( layer_week_text );
	
  fonts_unload_custom_font(time_font);
  fonts_unload_custom_font(date_font);

  window_destroy(window);

}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}

#include <pebble.h>
#include "swimming_window.h"

// BEGIN AUTO-GENERATED UI CODE; DO NOT MODIFY
static Window *s_window;
static GFont s_res_gothic_28_bold;
static GFont s_res_bitham_42_bold;
static TextLayer *main_display;
static TextLayer *upper_status_display;
static TextLayer *css_status_display;
static TextLayer *workout_stats_display;
static TextLayer *elapsed_time_display;
static TextLayer *lower_status_display;

static void initialise_ui(void) {
  s_window = window_create();
  #ifndef PBL_SDK_3
    window_set_fullscreen(s_window, true);
  #endif
  
  s_res_gothic_28_bold = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  s_res_bitham_42_bold = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  // main_display
  main_display = text_layer_create(GRect(2, 60, 140, 46));
  text_layer_set_text(main_display, "X/YY");
  text_layer_set_text_alignment(main_display, GTextAlignmentCenter);
  text_layer_set_font(main_display, s_res_bitham_42_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)main_display);
  
  // upper_status_display
  upper_status_display = text_layer_create(GRect(2, 35, 140, 28));
  text_layer_set_text(upper_status_display, "Int. X/XX rXX");
  text_layer_set_text_alignment(upper_status_display, GTextAlignmentCenter);
  text_layer_set_font(upper_status_display, s_res_gothic_28_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)upper_status_display);
  
  // css_status_display
  css_status_display = text_layer_create(GRect(2, 5, 140, 28));
  text_layer_set_text(css_status_display, "PXXm X:XX +X");
  text_layer_set_text_alignment(css_status_display, GTextAlignmentCenter);
  text_layer_set_font(css_status_display, s_res_gothic_28_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)css_status_display);
  
  // workout_stats_display
  workout_stats_display = text_layer_create(GRect(70, 105, 70, 30));
  text_layer_set_text(workout_stats_display, "0000m");
  text_layer_set_text_alignment(workout_stats_display, GTextAlignmentCenter);
  text_layer_set_font(workout_stats_display, s_res_gothic_28_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)workout_stats_display);
  
  // elapsed_time_display
  elapsed_time_display = text_layer_create(GRect(2, 135, 140, 30));
  text_layer_set_text(elapsed_time_display, "00:00:00");
  text_layer_set_text_alignment(elapsed_time_display, GTextAlignmentCenter);
  text_layer_set_font(elapsed_time_display, s_res_gothic_28_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)elapsed_time_display);
  
  // lower_status_display
  lower_status_display = text_layer_create(GRect(4, 105, 70, 30));
  text_layer_set_text(lower_status_display, "Pause");
  text_layer_set_text_alignment(lower_status_display, GTextAlignmentCenter);
  text_layer_set_font(lower_status_display, s_res_gothic_28_bold);
  layer_add_child(window_get_root_layer(s_window), (Layer *)lower_status_display);
}

static void destroy_ui(void) {
  window_destroy(s_window);
  text_layer_destroy(main_display);
  text_layer_destroy(upper_status_display);
  text_layer_destroy(css_status_display);
  text_layer_destroy(workout_stats_display);
  text_layer_destroy(elapsed_time_display);
  text_layer_destroy(lower_status_display);
}
// END AUTO-GENERATED UI CODE


static bool is_swimming = false;
static int pool_length = 25; // pool length in m *** make this adjustable in app 25/33/50
static int css_pace = 115; // seconds per 100m *** make this adjustable in app
static int current_interval = 0; // since this in array reference, goes from 0 to total_intervals-1
static int current_length;
static int total_lengths;
static int distance_swum = 0; // workout distance in m
static uint32_t length_time; // milliseconds for timer
static int rest_time;
static int current_rest_time = 0; //milliseconds for timer
static AppTimer *timer; //used to trigger end of length or rest events
static AppTimer *elapsed_timer; // used to track session time
static bool timer_triggered_by_button_press = false;
time_t timer_started; // if the clock is running, this holds the the last time the timer display was updated, if the clock is topped, this is 0
int elapsed_time; //the number of seconds the clock has been running

struct  {                //interval details in this structure
  int distance;          //distance each interval in m
  int pace_delta;        // difference from CSS in s/100m
  int rest;              // rest time in s
}interval[] = {
  {50, 0, 10},
  {100, 0, 20},
  {200, 6, 30},
  {200, 6, 30},
  {200, 6, 30},
  {200, 6, 30},
  {200, 5, 30},
  {200, 5, 30},
  {200, 5, 30},
  {200, 4, 30},
  {200, 4, 30},
  {200, 3, 30},
};

int total_intervals = sizeof(interval)/sizeof(interval[0]); //this is the trick to calculate the length of an array in C

// Custom vibration pattern for end of length short, short, long

static const uint32_t  end_of_length_vibe_segments[] = { 200, 200, 200, 200, 600 };
VibePattern end_length_vibe_pat = {
  .durations = end_of_length_vibe_segments,
  .num_segments = ARRAY_LENGTH(end_of_length_vibe_segments),
};

// Custom vibration pattern for end of interval long, long, long, long

static const uint32_t  end_of_interval_vibe_segments[] = { 600, 200, 600, 200, 600, 200, 600 };
VibePattern end_interval_vibe_pat = {
  .durations = end_of_interval_vibe_segments,
  .num_segments = ARRAY_LENGTH(end_of_interval_vibe_segments),
};


static void setup_interval(void) {
  
  static char interval_text_to_display [15];
  static char pace_text_to_display [20];
  
  //check we have a valid current_interval
  
  if (current_interval<0) current_interval = 0;
  if (current_interval > total_intervals-1) {current_interval = 0;} //At the moment, just go back to the start at the end of the intervals
  
  // set up the interval
  
  length_time = (css_pace + interval[current_interval].pace_delta) * pool_length * 10; // times 10 since we divide by 100m to get sec per m, then multiply by 1000 to get millseconds
  total_lengths = interval[current_interval].distance/pool_length;
  current_length = 0;
  rest_time = interval[current_interval].rest * 1000;
  
  // display pace information  
 
  snprintf(pace_text_to_display,sizeof(pace_text_to_display),"P%dm %d:%02d %+1d",pool_length, (css_pace / 60), (css_pace % 60), interval[current_interval].pace_delta);
  text_layer_set_text(css_status_display,pace_text_to_display);
  
  
  // display the interval details
  
  snprintf (interval_text_to_display, sizeof(interval_text_to_display),"Int. %d/%d r%d", current_interval+1, total_intervals, interval[current_interval].rest);
  text_layer_set_text(upper_status_display, interval_text_to_display);
  
};

static void update_elapsed_time_display(){
  
  static char time_to_display[9];
  time_t current_time;
  if (timer_started > 0){
    current_time = time(NULL);
    elapsed_time = elapsed_time + current_time - timer_started;
    timer_started = current_time;
    elapsed_timer = app_timer_register(1000, update_elapsed_time_display, NULL );
  }
  snprintf(time_to_display,sizeof(time_to_display),"%02d:%02d:%02d",elapsed_time / 3600, (elapsed_time / 60) % 60, elapsed_time  % 60 );
  text_layer_set_text(elapsed_time_display, time_to_display);
}

static void update_display(void){ //****** Rename this something specifc like display_current_length
  
  static char main_text_to_display [30];
  static char workout_status_text_to_display[30];

  
  if (is_swimming){
    text_layer_set_text(lower_status_display,"Swim");
  }
  else {
    text_layer_set_text(lower_status_display,"Paused");
  }
  
  snprintf (main_text_to_display, sizeof(main_text_to_display),"%d/%d", current_length, total_lengths);
  text_layer_set_text(main_display, main_text_to_display);
  
  snprintf(workout_status_text_to_display, sizeof(workout_status_text_to_display),"%dm",distance_swum);
  text_layer_set_text(workout_stats_display, workout_status_text_to_display);
}

static void rest(){
  
  static char text_to_display [30];
  static int current_rest_in_seconds;
  
//cancel the running timer
  
  app_timer_cancel(timer);
    
  
  if (current_rest_time > 0) {
    
    current_rest_in_seconds = current_rest_time / 1000;
    snprintf (text_to_display, sizeof(text_to_display),"%ds", current_rest_in_seconds); //****pull out these lines into a display_rest_time function
    text_layer_set_text(main_display, text_to_display);
    text_layer_set_text(lower_status_display,"Rest");
    timer = app_timer_register(1000, (AppTimerCallback) rest, NULL);
    current_rest_time = current_rest_time - 1000;
    
  }
  else{
    
    current_interval++;
    setup_interval();
    timer_trigger();
    
  }
  
}



 void timer_trigger(){
  
  // This function handles timer stop, start and interruption events */
    
  // first cancel any running timer */
  
  app_timer_cancel(timer);
  
  // If we are not swimming, that's all we need to do */
  // if we are swimming, signal end of length, add 1 to current length if there is another length in the set */
  
   if (!is_swimming) {
     update_display();
     if (current_rest_time > 0) {
       current_rest_time = 0;
       rest();
       return;
     }
   }
     

   if(is_swimming){

    
    //check to see if trigger was by a timer or a button press. If the, former, switch that switch, 
    // if the latter, make the end of length vibe signal
     
      if (timer_triggered_by_button_press) {
         timer_triggered_by_button_press = false;
        if (current_length == 0) {
          current_length = 1; //if timer triggered during rest, current_length will be 0, so set it to 1
        }
      }
      else {
        if (current_length < total_lengths) {
           vibes_enqueue_custom_pattern(end_length_vibe_pat);
        }
        else {
          vibes_enqueue_custom_pattern(end_interval_vibe_pat);
        }
        if (current_length>0) distance_swum = distance_swum + pool_length; //if you've arrived at the end of a length, not a rest, increment the length counter
        current_length++;
      }     
        
  
    // Start a new timer if we need to, or start the rest sequence
    
    
    if (current_length <= total_lengths) { 
      timer = app_timer_register(length_time, (AppTimerCallback) timer_trigger, NULL);
      update_display();
    }
    else {
      current_rest_time = rest_time;  
      current_length = 0; //sets length to 0 since 1 will be added when rest() calls timer_trigger()
      rest();
    }
   
  
   
   
   }
  
  }

  
 


static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
 
  /* Select button starts and pauses swimming */
  
  is_swimming = !is_swimming;
  timer_triggered_by_button_press = true;
  if (is_swimming) {
    timer_started = time(NULL);
    update_elapsed_time_display();
  }
  else {
    timer_started = 0;
  }
  timer_trigger();
    
  }
  

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {

  if (current_length>1) {
    current_length--;
  }
  else{
    current_interval--;
    setup_interval();
  }
  
  if (is_swimming) {
    if (current_rest_time == 0) {
      distance_swum = distance_swum - pool_length; // if swimming, assume you missed a length and subtract from total;
    }
    else {
       current_rest_time = rest_time;  // if in rest mode, restart the current rest
    }
    if (distance_swum < 0) distance_swum = 0; // prevent negative distance totals being forced.
    if (current_length == 0) current_length++; // if button press forces an interval change while still swimming, assume you are swimming length 1
  }
    update_display();
 
  }

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  
  if (current_length<total_lengths) {
    current_length++;
  }
  else {
    current_interval++;
    setup_interval();
  }
  
  if (is_swimming) {
    if (current_rest_time == 0) {
      distance_swum = distance_swum + pool_length; // if swimming, assume you finished a length and add to total;
    }
    else {
      current_rest_time = 0; // if resting, end the rest
    }
    if (current_length == 0) current_length++; // button press forces an interval change while still swimming, assume you are swimming length 1
  }
  
  update_display();
}

static void long_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  
    //do something
  is_swimming = false;
  update_display();
 
}

static void long_select_release_handler(ClickRecognizerRef recognizer, void *context) {
  
    //do something
  current_length = 50;
  update_display();
 
}



static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, long_select_click_handler, long_select_release_handler);
}
static void handle_window_unload(Window* window) {
  destroy_ui();
}

void show_swimming_window(void) {
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  window_set_click_config_provider(s_window,click_config_provider);
  window_stack_push(s_window, true);
  
}

void hide_swimming_window(void) {
  window_stack_remove(s_window, true);
}

static void init(void) {
  show_swimming_window();
  setup_interval();
  update_display();
}

static void deinit(void) {
  app_timer_cancel(timer);
  hide_swimming_window();
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  deinit();
}
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"

void world_menu_run_main_mode_0_thread(void) {
    world_menu_run_main_mode(OPTION_MENU_GAME_OPTIONS);
}

/* Runs the main-menu mode-0 thread in the slot below the current thread,
 * handing it this thread's first parameter, then waits for it and stops. */
/* Target 0x80106f64. */
void world_menu_run_main_mode_0_in_previous_thread(void) {
    s32 thread_id;

    thread_start(g_thread_current_id - 1, world_menu_run_main_mode_0_thread);
    thread_id = g_thread_current_id;
    thread_set_parameters(thread_id - 1, g_threads[thread_id].function_parameter_1, 0, 0);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}

/* Menu action: request the save/load screen from the world map, hand the
 * formation input state to the event input and exit the thread. */
void world_card_open_menu(void) {
    g_world_card_menu_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the tutorial categories from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_tutorial_categories(void) {
    g_world_tutorial_categories_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the fixed number panel from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_fixed_number_panel(void) {
    g_world_fixed_number_panel_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the debug variable list from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_debug_variable_list(void) {
    g_world_debug_variable_list_request_state = 1;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the formation screen from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_open(void) {
    g_world_formation_open_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Menu action: request the world map root level from the world map, hand the formation input
 * state to the event input and exit the thread. */
void world_formation_request_world_map_root(void) {
    g_world_map_root_request_state = WORLD_SCREEN_REQUEST_START_FADE_OUT;
    g_world_formation_menu_new_buttons = 0;
    script_load_event_input_state(&g_world_formation_menu_input_state);
    thread_exit_current();
}

/* Save the system flags and hide the world map's layers (dots, routes,
 * background, display objects, context value, date and location marker)
 * with the display off, for a full-screen menu. */
void world_formation_save_and_set_system_flags(void) {
    g_world_formation_saved_system_flags = g_main_system_flags;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES
        | MAIN_SYSTEM_FLAG_DISPLAY_OFF | MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE | MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS
        | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
        | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
}

void world_formation_restore_saved_system_flags(void) {
    g_main_system_flags = g_world_formation_saved_system_flags;
}

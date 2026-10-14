#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Initialize ATTACK resources and start map loading or deployment. */
s32 menu_init_attack_resources_and_threads(void) {
    s32 result;
    s32 return_value;
    game_options_fields_t* options;
    g_menu_hide_numeric_values = 0;
    menu_init_subsystems();
    unit_reset_animation_states();
    menu_reset_unit_records();
    if (g_script_attack_entry_mode == SCRIPT_ATTACK_ENTRY_PRELOADED) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOAD_USED;
    } else {
        g_thread_call_target = main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_ATTACK_OUT, 0x20000, g_main_heap_companion_overlay_load_address);
    }
    result = attack_load_scenario_conditionals();
    if (g_script_attack_entry_mode != SCRIPT_ATTACK_ENTRY_PRELOAD_USED) {
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
    g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
    g_menu_panel_fade_intensity = 0;
    if (world_script_check_tutorial_event_slot() != 0) {
        g_gfx_screen_fade = 0xff;
        g_menu_saved_game_options = g_main_game_options;
        options = &g_main_game_options.fields;
        options->cursor_movement = GAME_CURSOR_MOVEMENT_TYPE_A;
        options->multi_height_cursor_speed = GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR;
        options->navigation_messages = GAME_OPTION_ON;
        options->effect_messages = GAME_OPTION_OFF;
        options->target_flashing = GAME_OPTION_ON;
        options->show_unequippable_items = GAME_OPTION_ON;
    } else {
        g_gfx_screen_fade = 0;
    }
    attack_deploy_find_fieldable_units();
    if (result != 0) {
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, map_data_load_complete_thread);
        return_value = 1;
    } else {
        g_sound_music_track_1_id = 0;
        g_sound_music_track_2_id = 0;
        sound_play_music_tracks(MAIN_MUSIC_DEPLOYMENT, MAIN_MUSIC_NONE);
        attack_text_build_deployment_strings();
        attack_map_load_title_graphic();
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, attack_deploy_run_screen);
        return_value = 0;
    }
    return return_value;
}

#include "fft/event_attack.h"
#include "fft/world.h"

/*
 * Initialize ATTACK resources and select the thread that starts deployment.
 *
 * The tutorial path saves and overrides options, although the supported
 * target's tutorial-check helper currently returns 0. Return 1 selects the
 * map-load completion thread; 0 selects the ATTACK entry at 0x801c3f44.
 */
s32 world_script_init_attack_resources_and_threads(void) {
    s32 result;
    s32 return_value;
    game_options_fields_t* options;
    g_world_menu_hide_numeric_values = 0;
    world_menu_init_subsystems();
    world_unit_reset_animation_states();
    world_menu_reset_unit_records();
    if (g_world_script_attack_entry_mode == 2) {
        g_world_script_attack_entry_mode = 3;
    } else {
        g_world_thread_inner_subroutine_callback = main_file_load_checked_to_address;
        world_thread_call_on_main_stack(0x990, 0x20000, g_event_overlay_load_address);
    }
    result = attack_load_scenario_conditionals();
    if (g_world_script_attack_entry_mode != 3) {
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
    g_world_menu_panel_fade_mode = 0;
    g_world_menu_panel_fade_intensity = 0;
    if (world_script_check_tutorial_event_slot() != 0) {
        g_world_screen_fade = 0xff;
        /* Keep the typed saved-word store: a scalar store changes the final
         * options write's placement in the jump delay slot. */
        ((game_options_t*)&g_world_saved_game_options)->value = g_main_game_options_raw;
        options = (game_options_fields_t*)&g_main_game_options_raw;
        options->cursor_movement = GAME_CURSOR_MOVEMENT_TYPE_A;
        options->multi_height_cursor_speed = GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR;
        options->navigation_messages = GAME_OPTION_ON;
        options->effect_messages = GAME_OPTION_OFF;
        options->target_flashing = GAME_OPTION_ON;
        options->show_unequippable_items = GAME_OPTION_ON;
    } else {
        g_world_screen_fade = 0;
    }
    attack_deploy_find_fieldable_units();
    if (result != 0) {
        world_thread_start(6, world_map_data_load_complete_thread);
        return_value = 1;
    } else {
        g_world_sound_music_track_1_id = 0;
        g_world_sound_music_track_2_id = 0;
        world_sound_play_music_tracks(0x2a, 0);
        attack_text_build_deployment_strings();
        attack_map_load_title_graphic();
        world_thread_start(6, attack_deploy_run_screen);
        return_value = 0;
    }
    return return_value;
}

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_state.h"
#include "fft/data.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

#ifndef NULL
#    define NULL 0
#endif

typedef struct battle_weather_environment {
    u8 environment_id;
    u8 unknown_01;
} battle_weather_environment_t;

extern battle_weather_environment_t g_battle_event_weather_commands[];

/* Advances asynchronous battle-script, map, effect, and graphics requests. */
void battle_script_process_pending_requests(void) {
    s16 destruction_state;
    s16 effect_id;
    s16 jump_out_map_id;
    s16 jump_out_2_map_id;
    s16 jump_in_duration;
    s16 time_of_day;
    s16 weather_request;
    s32 weather_id;
    s32 loaded_slot;
    s32 save_slot;
    s32 saved_clear_slot;
    s32 loaded_clear_slot;
    s32 release_slot;
    s32 sfx_id;
    s32 unit_vram_copy;
    u16 reload_state;
    s32 append_state;
    u16 music_track_id;
    u32 volume_transition;
    s32 pending = 0;

    battle_unit_update_facing_and_pending_animations();
    jump_out_map_id = g_battle_event_pending_map_jump_out_id;
    if (jump_out_map_id != -1) {
        battle_state_start_map_jump_out(jump_out_map_id, 0x10);
    }
    jump_out_2_map_id = g_battle_event_pending_map_jump_out_2_id;
    if (jump_out_2_map_id != -1) {
        battle_state_start_change_map_jump_out(jump_out_2_map_id, 0x10);
    }
    jump_in_duration = g_battle_event_map_jump_in_request;
    if (jump_in_duration != 0) {
        battle_state_start_change_map_jump_in(jump_in_duration);
    }
    if (g_battle_3d_object_use_request != 0) {
        battle_map_command_set_3d_object_state(g_battle_event_map_command_80_arg1, g_battle_event_map_command_80_arg2);
        g_battle_3d_object_wait_status = 1;
    }
    if (g_battle_field_object_use_request != 0) {
        battle_map_command_start_texture_animation(
            g_battle_event_map_command_83_arg1, g_battle_event_map_command_83_arg2);
        g_battle_field_object_wait_status = 1;
    }
    if (g_battle_3d_object_wait_status != 0) {
        g_battle_3d_object_wait_status = battle_map_command_get_3d_object_state(g_battle_event_map_command_80_arg1);
    }
    if (g_battle_field_object_wait_status != 0) {
        g_battle_field_object_wait_status
            = battle_map_command_get_texture_animation_active(g_battle_event_map_command_83_arg1);
    }
    if (g_battle_map_refresh_pending != 0) {
        g_battle_map_refresh_pending = battle_map_load_mesh_variant(3);
    }
    reload_state = g_battle_map_reload_state;
    if (reload_state == 1) {
        if (battle_map_load_data_stage_0x75() == 0) {
            g_battle_map_reload_state = (u16)(g_battle_map_reload_state + 1);
        }
    } else if (reload_state == 2) {
        if (battle_map_load_data_stage_0x76() == 0) {
            g_battle_map_reload_state = (u16)(g_battle_map_reload_state + 1);
        }
    }
    append_state = g_battle_map_append_state;
    if ((append_state == 1) && (g_battle_map_destruction_wait_state == 0)) {
        battle_map_load_data(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), 0x99);
        g_battle_map_append_state = (u16)(g_battle_map_append_state + 1);
    } else if ((append_state == 2)
        && (battle_map_load_data(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), 0x9C) == 0x9B)) {
        g_battle_map_append_state = (u16)(g_battle_map_append_state + 1);
    }
    destruction_state = g_battle_map_destruction_wait_state;
    if (destruction_state == 2) {
        g_battle_map_destruction_wait_state = (s16)(destruction_state + 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
    } else if ((destruction_state == 3)
        && (battle_map_load_data(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), 0x76) == 0)) {
        g_battle_map_destruction_wait_state = 0;
    }
    weather_request = g_battle_event_weather_request;
    if (weather_request != -1) {
        weather_id = weather_request & 0xF;
        if (!(weather_request & 0xF00)) {
            g_battle_event_weather_request = -1;
        } else {
            g_battle_event_weather_request = weather_id;
            if (g_battle_map_weather_flags & 1) {
                g_battle_event_weather_request = (s16)(weather_id + 5);
            }
            battle_map_set_weather_texture_overlay(
                g_battle_event_weather_commands[g_battle_event_weather_request].environment_id);
        }
    }
    time_of_day = g_battle_event_pending_map_state;
    if (time_of_day != -1) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, time_of_day);
        battle_map_set_weather_texture_overlay(0x8BU);
    }
    effect_id = g_battle_event_pending_effect_id;
    if (effect_id != -1) {
        if (g_battle_event_effect_target_mode == 2) {
            battle_effect_start_new(effect_id, 2, (u8*)(u32)g_battle_event_effect_target_misc_id);
        } else {
            battle_effect_start_new(effect_id, 0, (u8*)(u32)g_battle_event_effect_target_misc_id);
        }
    }
    if (g_battle_unit_graphics_load_pending != 0) {
        g_battle_unit_graphics_load_pending = battle_gfx_poll_unit_graphics_load();
    }
    if (pending == 0) {
        loaded_slot = g_battle_event_pending_loaded_evtchr_slot;
        if (loaded_slot >= 0) {
            battle_gfx_unpack_evtchr_file_to_vram_slot(loaded_slot, g_battle_event_loaded_evtchr_buffer);
        }
        save_slot = g_battle_event_pending_evtchr_save_slot;
        if (save_slot >= 0) {
            battle_gfx_save_evtchr_slot(save_slot);
        }
        saved_clear_slot = g_battle_event_pending_saved_evtchr_clear_slot;
        if (saved_clear_slot >= 0) {
            battle_gfx_clear_saved_evtchr_slot(saved_clear_slot);
        }
        loaded_clear_slot = g_battle_event_pending_loaded_evtchr_clear_slot;
        if (loaded_clear_slot >= 0) {
            battle_gfx_clear_loaded_evtchr_slot(loaded_clear_slot);
        }
        unit_vram_copy = g_battle_event_pending_unit_vram_copy;
        if (unit_vram_copy >= 0) {
            battle_gfx_load_misc_unit_into_evtchr_slot((u32)(unit_vram_copy & 0xFF00) >> 8, unit_vram_copy & 0xFF);
        }
        release_slot = g_battle_event_pending_reserved_vram_release_slot;
        if (release_slot >= 0) {
            battle_gfx_release_reserved_vram_slot(release_slot);
        }
        sfx_id = g_sound_effect_id_to_play;
        if (sfx_id != -1) {
            main_sound_play_sfx(sfx_id);
        }
        if (g_battle_music_switch_request >= 0) {
            main_sound_stop_forced_music();
            g_battle_current_music_track_index = (u16)(g_battle_current_music_track_index ^ 1);
            if (g_battle_current_music_track_index == 0) {
                music_track_id = g_battle_music_track_1_id;
            } else {
                music_track_id = g_battle_music_track_2_id;
            }
            if (music_track_id != 0) {
                main_sound_switch_music_track(g_battle_current_music_track_index + 1,
                    battle_script_scale_music_volume(g_battle_event_music_switch_volume),
                    g_battle_event_music_switch_fade * 4);
            }
        }
        if (g_battle_music_unload_slot_request >= 0) {
            main_sound_stop_forced_music();
            main_sound_unload_scenario_mus(g_battle_music_unload_slot_request);
        }
        if (g_battle_music_set_track_request != -1) {
            main_sound_unload_current_scenario_music();
            main_sound_set_current_music_track(g_battle_music_set_track_request);
        }
        volume_transition = g_battle_music_volume_transition_request;
        if (volume_transition != -1U) {
            main_sound_set_current_music_target(volume_transition & 0xFFFF, (volume_transition >> 0xE) & 0x3FFC);
        }
    }
}

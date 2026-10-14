#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "psx/types.h"

#ifndef NULL
#    define NULL 0
#endif

/* Advances asynchronous battle-script, map, effect, and graphics requests. */
void script_process_pending_requests(void) {
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
    effect_start_target_data_t effect_target;

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
        map_command_set_3d_object_state(g_battle_event_3d_object_id, g_battle_event_3d_object_state);
        g_battle_3d_object_wait_status = 1;
    }
    if (g_battle_field_object_use_request != 0) {
        map_command_start_texture_animation(g_battle_event_map_command_83_arg1, g_battle_event_map_command_83_arg2);
        g_battle_field_object_wait_status = 1;
    }
    if (g_battle_3d_object_wait_status != 0) {
        g_battle_3d_object_wait_status = map_command_get_3d_object_state(g_battle_event_3d_object_id);
    }
    if (g_battle_field_object_wait_status != 0) {
        g_battle_field_object_wait_status
            = map_command_get_texture_animation_active(g_battle_event_map_command_83_arg1);
    }
    if (g_map_refresh_pending != 0) {
        g_map_refresh_pending = map_load_mesh_variant(3);
    }
    reload_state = g_map_reload_state;
    if (reload_state == 1) {
        if (map_start_data_load() == 0) {
            g_map_reload_state = g_map_reload_state + 1;
        }
    } else if (reload_state == 2) {
        if (map_poll_data_load() == 0) {
            g_map_reload_state = g_map_reload_state + 1;
        }
    }
    append_state = g_map_append_state;
    if ((append_state == 1) && (g_map_destruction_wait_state == 0)) {
        map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_START_HELD);
        g_map_append_state = g_map_append_state + 1;
    } else if ((append_state == 2)
        && (map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_POLL_HELD)
            == MAP_LOAD_RESULT_HELD)) {
        g_map_append_state = g_map_append_state + 1;
    }
    destruction_state = g_map_destruction_wait_state;
    if (destruction_state == 2) {
        g_map_destruction_wait_state = destruction_state + 1;
        map_dispatch_map_data_command(MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
    } else if ((destruction_state == 3)
        && (map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), MAP_LOAD_COMMAND_POLL) == 0)) {
        g_map_destruction_wait_state = 0;
    }
    weather_request = g_battle_event_weather_request;
    if (weather_request != -1) {
        weather_id = weather_request & 0xF;
        if (!(weather_request & 0xF00)) {
            g_battle_event_weather_request = -1;
        } else {
            g_battle_event_weather_request = weather_id;
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                g_battle_event_weather_request = weather_id + 5;
            }
            map_run_weather_command(g_battle_event_weather_commands[g_battle_event_weather_request].environment_id);
        }
    }
    time_of_day = g_battle_event_pending_map_state;
    if (time_of_day != -1) {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, time_of_day);
        map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
    }
    effect_id = g_battle_event_pending_effect_id;
    if (effect_id != -1) {
        if (g_battle_event_effect_target_mode == EFFECT_START_ON_CASTER) {
            effect_target.runtime_id = g_battle_event_effect_target_runtime_id;
            effect_start_new(effect_id, EFFECT_START_ON_CASTER, effect_target);
        } else {
            effect_target.runtime_id = g_battle_event_effect_target_runtime_id;
            effect_start_new(effect_id, EFFECT_START_ON_UNIT, effect_target);
        }
    }
    if (g_battle_unit_graphics_load_pending != 0) {
        g_battle_unit_graphics_load_pending = battle_gfx_poll_unit_graphics_load();
    }
    if (pending == 0) {
        loaded_slot = g_script_pending_loaded_evtchr_slot;
        if (loaded_slot >= 0) {
            battle_gfx_unpack_evtchr_file_to_vram_slot(loaded_slot, g_script_loaded_evtchr_buffer);
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
            battle_gfx_load_unit_attack_image_into_evtchr_slot((unit_vram_copy & 0xFF00) >> 8, unit_vram_copy & 0xFF);
        }
        release_slot = g_battle_event_pending_reserved_vram_release_slot;
        if (release_slot >= 0) {
            battle_gfx_release_reserved_vram_slot(release_slot);
        }
        sfx_id = g_sound_pending_sfx_id;
        if (sfx_id != -1) {
            main_sound_play_sfx(sfx_id);
        }
        if (g_sound_music_switch_request >= 0) {
            main_sound_stop_current_music();
            g_sound_music_track_index = g_sound_music_track_index ^ 1;
            if (g_sound_music_track_index == 0) {
                music_track_id = g_sound_music_track_1_id;
            } else {
                music_track_id = g_sound_music_track_2_id;
            }
            if (music_track_id != 0) {
                main_sound_switch_music_track(g_sound_music_track_index + 1,
                    script_scale_music_volume(g_battle_event_music_switch_volume),
                    g_battle_event_music_switch_fade * 4);
            }
        }
        if (g_sound_music_unload_slot_request >= 0) {
            main_sound_stop_current_music();
            main_sound_unload_scenario_mus(g_sound_music_unload_slot_request);
        }
        if (g_sound_music_set_track_request != -1) {
            main_sound_unload_current_scenario_music();
            main_sound_set_current_music_track(g_sound_music_set_track_request);
        }
        volume_transition = g_sound_music_volume_transition_request;
        if (volume_transition != -1U) {
            main_sound_set_current_music_target(volume_transition & 0xFFFF, (volume_transition >> 0xE) & 0x3FFC);
        }
    }
}

#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

typedef struct wldcore_deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 choices[8];
} wldcore_deep_dungeon_battle_t;

extern wldcore_deep_dungeon_battle_t* g_wldcore_deep_dungeon_battles;
void world_build_at_list_2(void);

s32 rand(void);
void wldcore_bar_push_menu_level(void);
void wldcore_menu_run_shop_and_rebuild_screen(s32 file_slot);
void wldcore_window_set_render_state_2_from_arg_0x10(s32* level);

/* Per-frame step of the location menu level: fades its two windows in, starts
 * the menu thread after the delay and handles input. A confirmed entry opens
 * the bar, a shop, a deep-dungeon battle chosen by the location's conditional
 * set or the selected destination map; the help button shows the entry's
 * message and cancel closes the level. */
void wldcore_menu_step_location_menu_level(wldcore_menu_location_menu_level_t* level) {
    s16* selected;
    u8 unused[0x28];
    s16* values;
    s32 value;
    s32 text;
    s32 offset;
    wldcore_deep_dungeon_battle_t* records;
    wldcore_deep_dungeon_battle_t* record;

    if (level->fade != 0) {
        if (level->fade >= 0x80) {
            level->fade = 0x80;
        }
        g_wldcore_window_record_colors[level->slots.window_a].red = level->fade;
        g_wldcore_window_record_colors[level->slots.window_a].green = level->fade;
        g_wldcore_window_record_colors[level->slots.window_a].blue = level->fade;
        g_wldcore_window_record_colors[level->slots.window_b].red = level->fade;
        g_wldcore_window_record_colors[level->slots.window_b].green = level->fade;
        g_wldcore_window_record_colors[level->slots.window_b].blue = level->fade;
        if (level->fade == 0x80) {
            level->fade = 0;
            wldcore_gfx_toggle_captured_world_frame(1);
        } else {
            level->fade += 7;
        }
        return;
    }
    if (level->delay != 0) {
        if (level->delay == 8) {
            world_thread_start(0xC, world_build_at_list_2);
            world_thread_set_parameters(0xC, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        }
        level->delay--;
        return;
    }
    if (g_main_system_flags & 8) {
        return;
    }
    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        wldcore_gfx_toggle_captured_world_frame(0);
        g_main_system_flags = (g_main_system_flags | 4) & 0xF7FFFFFD;
        if (level->slots.result != 0) {
            wldcore_list_store_cursor_state(0);
            selected = &g_wldcore_window_panel_render_state.selected_index;
            values = g_wldcore_list_entry_values;
            value = values[*selected];
            if ((value & 0xF800) == 0xB800) {
                g_main_system_flags ^= 4;
                value &= 0x7FF;
                switch (value) {
                case 0x5D:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_sound_enqueue_music_start(0x221);
                    wldcore_bar_push_menu_level();
                    return;
                case 0x5E:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(0);
                    return;
                case 0x5F:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(0x65);
                    return;
                case 0x60:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(0x64);
                    return;
                default:
                    world_script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION, value - 0xED);
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    if (wldcore_script_process_conditional_set(g_wldcore_map_projection_state.marker.kind, 0x20) != 0) {
                        offset = g_wldcore_script_state.args[0] * 10;
                        records = g_wldcore_deep_dungeon_battles;
                        /* The byte offset and shared base preserve the target's choice-index calculation. */
                        record = (wldcore_deep_dungeon_battle_t*)((u8*)records + offset);
                        g_wldcore_next_map_id[0] = record->map_id;
                        world_script_set_variable(
                            EVENT_SCRIPT_VAR_CURRENT_ENTD, ((u8*)records)[((rand() * 8) >> 15) + offset + 2]);
                        world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                        world_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, record->squad_id);
                        world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[1]);
                        world_script_set_variable(
                            EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, (rand() * g_wldcore_script_state.args[2]) >> 15);
                        g_main_system_flags = (g_main_system_flags | 0x2000000) ^ 1;
                    } else {
                        wldcore_fade_start_screen(0, 0x10);
                        wldcore_sound_enqueue_music_start(0x11B);
                    }
                    return;
                }
            }
            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[*selected]);
            g_main_system_flags ^= 1;
            value = values[*selected] & 0x7FF;
            g_wldcore_next_map_id[0] = value + 1;
            return;
        }
        g_main_system_flags = (g_main_system_flags | 2) & ~4;
        wldcore_list_clear_cursor_state(0);
        wldcore_menu_close_level_and_release_records(&level->slots, 1);
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & 0xF800) == 0xB800) {
            wldcore_sound_play_effect(MAIN_SFX_CANCEL);
            level->slots.result = 0;
            world_thread_set_parameters(0xC, 0, 0, 1);
            g_main_system_flags |= 4;
        }
    } else if (g_wldcore_new_button_presses & PSX_PAD_SELECT) {
        wldcore_window_set_render_state_2_from_arg_0x10((s32*)level);
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & 0xF800) == 0xA800) {
            wldcore_menu_push_message_level((value & 0x7FF) + 0x8801, 1);
        } else {
            value &= 0x7FF;
            text = value + 0xFEE;
            if (value >= 0xED) {
                text = value + 0xFA3;
            }
            wldcore_menu_push_message_level(text, 1);
        }
    } else if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_fade_start_screen(2, 0x10);
        wldcore_sound_enqueue_audio_command(2, 0x10);
        wldcore_sound_enqueue_audio_command(4, 2);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->slots.result = 1;
        world_thread_set_parameters(0xC, 0, 0, 1);
        g_main_system_flags |= 4;
    }
}

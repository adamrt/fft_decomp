#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

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
        g_wldcore_window_records[level->slots.window_a].color.r = level->fade;
        g_wldcore_window_records[level->slots.window_a].color.g = level->fade;
        g_wldcore_window_records[level->slots.window_a].color.b = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.r = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.g = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.b = level->fade;
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
            thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        }
        level->delay--;
        return;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        wldcore_gfx_toggle_captured_world_frame(0);
        g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
            & ~(MAIN_SYSTEM_FLAG_DRAW_MAP | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION);
        if (level->slots.result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU);
            selected = &g_wldcore_window_panel_render_state.selected_index;
            values = g_wldcore_list_entry_values;
            value = values[*selected];
            if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
                g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                value &= TEXT_ID_ENTRY_MASK;
                switch (value) {
                case WLDCORE_TEXT_ID_BAR& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BAR));
                    wldcore_bar_push_menu_level();
                    return;
                case WLDCORE_TEXT_ID_SHOP& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(0);
                    return;
                case WLDCORE_TEXT_ID_SOLDIER_OFFICE& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(WORLD_MENU_CONTEXT_SOLDIER_OFFICE);
                    return;
                case WLDCORE_TEXT_ID_FUR_SHOP& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(WORLD_MENU_CONTEXT_FUR_SHOP);
                    return;
                default:
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION,
                        value - (WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK));
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    if (wldcore_script_process_conditional_set(
                            g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY)
                        != 0) {
                        offset = g_wldcore_script_state.args[0] * sizeof(wldcore_deep_dungeon_battle_t);
                        records = g_wldcore_deep_dungeon_battles;
                        record = &records[g_wldcore_script_state.args[0]];
                        g_wldcore_next_map_id[0] = record->map_id;
                        /* Direct ENTD indexing folds the table base and removes a retail addition. */
                        script_set_variable(
                            EVENT_SCRIPT_VAR_CURRENT_ENTD, (&records->entd_ids[0])[((rand() * 8) >> 15) + offset]);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, record->squad_id);
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[1]);
                        script_set_variable(
                            EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, (rand() * g_wldcore_script_state.args[2]) >> 15);
                        g_main_system_flags
                            = (g_main_system_flags | MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) ^ MAIN_SYSTEM_FLAG_RUNNING;
                    } else {
                        wldcore_fade_start_screen(0, 0x10);
                        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
                    }
                    return;
                }
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[*selected]);
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_RUNNING;
            value = values[*selected] & TEXT_ID_ENTRY_MASK;
            g_wldcore_next_map_id[0] = value + 1;
            return;
        }
        g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_DRAW_MAP) & ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU);
        wldcore_menu_close_level_and_release_records(&level->slots, 1);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
            wldcore_sound_play_effect(MAIN_SFX_CANCEL);
            level->slots.result = 0;
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_SELECT) {
        wldcore_window_set_secondary_render_palette_2(&level->slots);
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_MAP_NAME_BASE) {
            wldcore_menu_push_message_level((value & TEXT_ID_ENTRY_MASK) + 0x8801, 1);
        } else {
            value &= TEXT_ID_ENTRY_MASK;
            text = value + 0xFEE;
            if (value >= (WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK)) {
                text = value + 0xFA3;
            }
            wldcore_menu_push_message_level(text, 1);
        }
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->slots.result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

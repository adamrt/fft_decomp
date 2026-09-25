/*
 * WORLD twin:
 * world_menu_auto_battle_setting_thread (same code, globals and callees
 * rebound).
 */
#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/menu.h"
#include "fft/option.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern void battle_update_menu_cursor_primitives(
    world_menu_icon_thread_param_t* param, world_menu_icon_sprites_t* record, s32 frame, s32 cursor);
struct battle_menu_frame_primitives;
extern void battle_menu_submit_frame_primitives(struct battle_menu_frame_primitives* menu);
extern void battle_menu_update_selection_from_input(battle_menu_idle_action_entry_t* bounds, s32* cursor);
struct menu_frame_sprites;
extern void battle_menu_configure_frame_cluts(struct menu_frame_sprites* icons);

/* Auto-battle setting menu thread.
 *
 * The cursor starts at the attacker's auto_battle_setting (0, 0xC, 0xE,
 * 0x10 map to options 1-4 after "none" at 0). Whenever circle is pressed the
 * five option texts (0x5009-0x500D) are redrawn with the selected one
 * highlighted; choosing option 0 clears the setting and restarts the idle
 * action menu if the unit's menu id changes. */
void battle_menu_auto_battle_setting_thread(void) {
    RECT rect;
    battle_menu_window_record_t records[2];
    s32 cursor;
    s32 previous;
    battle_stats_t* stats;
    world_menu_icon_thread_param_t* param;
    void* buffer;
    s32 i;
    s32 j;
    s32 fresh;
    s32 before;
    s32 after;
    s32 selected;
    s16* colour;
    battle_menu_window_record_t* record;

    param = (world_menu_icon_thread_param_t*)battle_thread_get_current_parameter_1();
    cursor = param->cursor;
    stats = battle_unit_get_attacker_data_pointer();
    if (stats->auto_battle_setting == 0) {
        cursor = 0;
    } else if (stats->auto_battle_setting == 0xC) {
        cursor = 1;
    } else if (stats->auto_battle_setting == 0xE) {
        cursor = 2;
    } else if (stats->auto_battle_setting == 0x10) {
        cursor = 3;
    } else {
        cursor = 4;
    }
    previous = cursor;
    battle_menu_build_window_sprites(
        (battle_menu_window_header_t*)&rect, (battle_menu_window_spec_t*)param, &records[0]);
    battle_copy_bytes(&records[1], &records[0], 0x7C);
    fresh = 0;
    i = 0;
    colour = g_battle_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_battle_script_event_input & PSX_PAD_CIRCLE)) {
            buffer = battle_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
            if (((g_battle_script_event_input & PSX_PAD_CIRCLE) && battle_thread_is_previous_running() == 0)
                || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = battle_menu_get_id_based_on_mount_moveable_actable(g_battle_active_turn_unit.battle_id);
                    stats->auto_battle_setting = 0;
                    g_battle_menu_pending_selection[0] = 0x12;
                    colour[0] = 8;
                    colour[1] = 4;
                    colour[2] = 4;
                    colour[3] = 4;
                    colour[4] = 4;
                    after = battle_menu_get_id_based_on_mount_moveable_actable(g_battle_active_turn_unit.battle_id);
                    if (before != after) {
                        battle_thread_set_parameters(8, 0, 0, 1);
                        battle_menu_build_idle_action_menu(after);
                    }
                } else {
                    colour[0] = 4;
                    colour[1] = 0;
                    colour[2] = 0;
                    colour[3] = 0;
                    colour[4] = 0;
                    colour[selected] = 8;
                }
                g_battle_menu_auto_battle_row_actions[1] = 0x27;
                g_battle_menu_auto_battle_row_actions[2] = 0x27;
                g_battle_menu_auto_battle_row_actions[3] = 0x27;
                g_battle_menu_auto_battle_row_actions[4] = 0x27;
                if (cursor == previous) {
                    g_battle_menu_auto_battle_row_actions[1] = 2;
                    g_battle_menu_auto_battle_row_actions[2] = 2;
                }
                previous = cursor;
                g_battle_menu_thread_menu_data[2].selected_index = 0;
                battle_menu_handle_action(param, previous);
            }
            for (j = 0; j < 5; j++) {
                g_menu_text_palette_offset = colour[j];
                g_menu_text_state.stride = param->width;
                /* The target passes x and y as full words; the s16 prototype narrows them. */
                ((void (*)(s32, s32))battle_menu_set_text_origin)(8, j * 16 + 9);
                battle_menu_display_text_entry(j + 0x5009, buffer, &g_menu_text_state.origin_x);
            }
            LoadImage(&rect, buffer);
            fresh = 1;
        }
        battle_thread_yield();
        if (fresh != 0) {
            battle_menu_free_memory(buffer);
            fresh = 0;
        }
        param->cursor = cursor;
        if (battle_menu_should_close_thread(&g_battle_script_event_input) != 0) {
            break;
        }
        battle_menu_update_selection_from_input((battle_menu_idle_action_entry_t*)param, &cursor);
        battle_handle_menu_cancel_input(param);
        battle_menu_configure_frame_cluts((struct menu_frame_sprites*)record);
        battle_update_menu_cursor_primitives(param, (world_menu_icon_sprites_t*)record, i, cursor);
        battle_menu_submit_frame_primitives((struct battle_menu_frame_primitives*)record);
    }
    battle_thread_yield();
    battle_thread_exit_current();
}

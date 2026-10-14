#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

/* Run the five-policy auto-battle menu and update its action rows. */
void menu_auto_battle_setting_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    s32 cursor;
    s32 previous;
    unit_stats_t* stats;
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 j;
    s32 fresh;
    s32 before;
    s32 after;
    s32 selected;
    s16* color;
    menu_icon_record_t* record;

    param = (menu_entry_t*)battle_thread_get_current_parameter_1();
    cursor = param->selected_index;
    stats = menu_get_selected_unit_stats();
    if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
        cursor = 0;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_TARGETED) {
        cursor = 1;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_RECOVERY) {
        cursor = 2;
    } else if (stats->auto_battle_setting == AI_AUTOBATTLE_FADING_LIFE) {
        cursor = 3;
    } else {
        cursor = 4;
    }
    previous = cursor;
    battle_menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));
    fresh = 0;
    i = 0;
    color = g_menu_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_script_event_input & PSX_PAD_CIRCLE)) {
            buffer = battle_menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            if (((g_script_event_input & PSX_PAD_CIRCLE) && battle_thread_is_previous_running() == 0) || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = battle_menu_get_id_based_on_mount_moveable_actable(g_menu_selected_unit.battle_id);
                    stats->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                    g_menu_pending_selection[0] = 0x12;
                    color[0] = 8;
                    color[1] = 4;
                    color[2] = 4;
                    color[3] = 4;
                    color[4] = 4;
                    after = battle_menu_get_id_based_on_mount_moveable_actable(g_menu_selected_unit.battle_id);
                    if (before != after) {
                        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
                        battle_menu_build_idle_action_menu(after);
                    }
                } else {
                    color[0] = 4;
                    color[1] = 0;
                    color[2] = 0;
                    color[3] = 0;
                    color[4] = 0;
                    color[selected] = 8;
                }
                g_menu_auto_battle_row_actions[1] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[2] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[3] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                g_menu_auto_battle_row_actions[4] = BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM;
                if (cursor == previous) {
                    g_menu_auto_battle_row_actions[1] = BATTLE_MENU_ID_AUTO_BATTLE_TARGET;
                    g_menu_auto_battle_row_actions[2] = BATTLE_MENU_ID_AUTO_BATTLE_TARGET;
                }
                previous = cursor;
                g_menu_thread_menu_data[BATTLE_MENU_ID_AUTO_BATTLE_TARGET].selected_index = 0;
                menu_handle_action(param, previous);
            }
            for (j = 0; j < 5; j++) {
                g_menu_text_palette_offset = color[j];
                g_menu_text_state.pen.stride = param->inner_width;
                menu_set_text_origin(8, j * 16 + 9);
                menu_display_text_entry(j + 0x5009, buffer, &g_menu_text_state.pen);
            }
            LoadImage(&rect, (u32*)buffer);
            fresh = 1;
        }
        thread_yield();
        if (fresh != 0) {
            menu_free_memory(buffer);
            fresh = 0;
        }
        param->selected_index = cursor;
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_step_wrapping_cursor_on_scroll_buttons(param, &cursor);
        menu_handle_cancel_input(param);
        menu_select_icon_cluts(&record->base);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        menu_submit_icon_primitives(&record->base);
    }
    thread_yield();
    thread_exit_current();
}

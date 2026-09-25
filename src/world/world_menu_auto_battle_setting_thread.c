#include "fft/battle.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* param, s32 value);
typedef struct world_menu_icon_prims world_menu_icon_prims_t;
extern void world_menu_submit_icon_primitives(world_menu_icon_prims_t* menu);
extern s16 g_world_menu_pending_selection[];

/* Formation-screen auto-battle setting menu thread.
 *
 * The cursor starts at the stored unit's auto_battle_setting (0, 0xC, 0xE,
 * 0x10 map to options 1-4 after "none" at 0). Whenever circle is pressed the
 * five option texts (0x5009-0x500D) are redrawn with the selected one
 * highlighted; choosing option 0 clears the setting and restarts the idle
 * action menu if the unit's menu id changes. */
void world_menu_auto_battle_setting_thread(void) {
    RECT rect;
    world_menu_icon_record_t records[2];
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
    world_menu_icon_record_t* record;

    param = world_thread_get_current_parameter_1();
    cursor = param->cursor;
    stats = world_unit_get_battle_stats_for_stored();
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
    world_menu_build_icon_record(&rect, param, &records[0]);
    world_script_copy_bytes(&records[1], &records[0], 0x7C);
    fresh = 0;
    i = 0;
    colour = g_world_auto_battle_option_colors;
    for (;; i++) {
        record = &records[i & 1];
        if (i == 0 || (g_world_menu_new_button_input & PSX_PAD_CIRCLE)) {
            buffer = world_menu_build_and_upload_window_frame_image(param->width, param->height, &rect, 1);
            if (((g_world_menu_new_button_input & PSX_PAD_CIRCLE) && world_thread_is_previous_running() == 0)
                || i == 0) {
                selected = cursor;
                if (selected == 0) {
                    before = battle_menu_get_id_based_on_mount_moveable_actable(g_world_unit_view_battle_id);
                    stats->auto_battle_setting = 0;
                    g_world_menu_pending_selection[0] = 0x12;
                    colour[0] = 8;
                    colour[1] = 4;
                    colour[2] = 4;
                    colour[3] = 4;
                    colour[4] = 4;
                    after = battle_menu_get_id_based_on_mount_moveable_actable(g_world_unit_view_battle_id);
                    if (before != after) {
                        world_thread_set_parameters(8, 0, 0, 1);
                        /* The target passes a0 to this argument-less stub. */
                        ((void (*)(s32))world_menu_build_idle_action_stub)(after);
                    }
                } else {
                    colour[0] = 4;
                    colour[1] = 0;
                    colour[2] = 0;
                    colour[3] = 0;
                    colour[4] = 0;
                    colour[selected] = 8;
                }
                g_world_menu_auto_battle_row_actions[1] = 0x27;
                g_world_menu_auto_battle_row_actions[2] = 0x27;
                g_world_menu_auto_battle_row_actions[3] = 0x27;
                g_world_menu_auto_battle_row_actions[4] = 0x27;
                if (cursor == previous) {
                    g_world_menu_auto_battle_row_actions[1] = 2;
                    g_world_menu_auto_battle_row_actions[2] = 2;
                }
                previous = cursor;
                g_world_menu_thread_menu_data[2].selected_index = 0;
                world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)param, previous);
            }
            for (j = 0; j < 5; j++) {
                g_world_menu_text_color = colour[j];
                g_world_menu_text_state.stride = param->width;
                /* The target passes both coordinates without the s16 sign extension. */
                ((void (*)(s32, s32))world_menu_set_text_origin)(8, j * 16 + 9);
                world_menu_display_text_entry(j + 0x5009, buffer, &g_world_menu_text_state.origin_x);
            }
            LoadImage(&rect, buffer);
            fresh = 1;
        }
        world_thread_yield();
        if (fresh != 0) {
            world_menu_free_memory(buffer);
            fresh = 0;
        }
        param->cursor = cursor;
        if (world_menu_check_thread_completion(&g_world_menu_new_button_input) != 0) {
            break;
        }
        world_menu_step_wrapping_cursor_on_scroll_buttons((world_menu_wrapping_cursor_bounds_t*)param, &cursor);
        world_menu_cancel_thread_group((world_menu_cancel_context_t*)param);
        world_menu_select_icon_cluts(&record->base);
        world_menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        world_menu_submit_icon_primitives((world_menu_icon_prims_t*)record);
    }
    world_thread_yield();
    world_thread_exit_current();
}

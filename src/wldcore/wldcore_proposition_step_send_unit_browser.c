#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

u32 wldcore_input_check_repeating_directional(u32 buttons);
void wldcore_menu_push_layout_window_level(void);
void wldcore_unit_build_status_panel_data(s32 party_index);
void world_build_at_list_2(void);

/* Location-browser step of the proposition send-unit level (modes 3-5,
 * entered from wldcore_menu_step_send_unit_level).
 *
 * Mode 3 waits for WORLD threads 12 and 14, shows the two arrow windows,
 * enters mode 4 and starts the background threads for the highlighted list
 * entry. Mode 4 is interactive: 0x60 hides the arrows, stops threads 8, 12
 * and 9 and enters mode 5; 0x100 parks both arrows in palette 10 and pushes a
 * layout-window level; right (8) and left (4) wrap the slot through
 * unit_count, repaint the arrow sprites 0x66-0x69, set the scroll direction in
 * thread 8's block and reload the entry's text. Mode 5 waits for threads 8,
 * 12 and 9, clears system flags 0x20000 and 0x800, restarts threads 14 and 12
 * and shows the six records hidden on entry.
 *
 * Twin of the participant browser wldcore_menu_handle_participant_detail_input; the chained stores and the
 * shared scroll tail follow it. */
void wldcore_proposition_step_send_unit_browser(wldcore_menu_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;
    s32 window;
    s16* menu_selected_index;

    if (level->mode == 3) {
        if (world_thread_is_running(0xC) != 0) {
            return;
        }
        if (world_thread_is_running(0xE) != 0) {
            return;
        }
        g_wldcore_window_records[level->left_arrow].flags ^= 0x10;
        g_wldcore_window_records[level->right_arrow].flags ^= 0x10;
        level->mode = 4;
        menu_selected_index = &g_wldcore_window_panel_render_state.selected_index;
        wldcore_start_core_background_threads(g_wldcore_list_entry_values[*menu_selected_index] & 0x7FF);
        level->slot = *menu_selected_index;
    } else if (level->mode == 4) {
        buttons = g_wldcore_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_wldcore_window_records[level->left_arrow].flags |= 0x10;
            g_wldcore_window_records[level->right_arrow].flags |= 0x10;
            level->mode = 5;
            world_thread_set_parameters(8, 0, 0, 1);
            world_thread_set_parameters(0xC, 0, 0, 1);
            world_thread_set_parameters(9, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette
                = 0xA;
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            wldcore_menu_push_layout_window_level();
            return;
        }
        if (!(g_wldcore_current_button_input & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->left_arrow].sequence = 0x66;
            g_wldcore_window_records[level->right_arrow].sequence = 0x68;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter
                = g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
        }
        if (wldcore_input_check_repeating_directional(PSX_PAD_R1) != 0
            && g_wldcore_window_records[level->left_arrow].sequence != 0x67) {
            g_wldcore_window_records[level->right_arrow].sequence = 0x69;
            window = level->right_arrow;
            g_wldcore_window_records[window].anim_counter = 0;
            g_wldcore_window_records[window].frame_index = 0;
            if (level->unit_count == 1) {
                /* Shared tail: duplicating the sound call per arm is only
                 * partly cross-jumped and leaves an extra copy. */
                goto done;
            }
            selected = level->slot + 1;
            if (selected >= level->unit_count) {
                level->slot = 0;
            } else {
                level->slot = selected;
            }
            direction = 1;
        } else {
            if (wldcore_input_check_repeating_directional(PSX_PAD_L1) == 0
                || g_wldcore_window_records[level->right_arrow].sequence == 0x69) {
                return;
            }
            g_wldcore_window_records[level->left_arrow].sequence = 0x67;
            window = level->left_arrow;
            g_wldcore_window_records[window].anim_counter = 0;
            g_wldcore_window_records[window].frame_index = 0;
            if (level->unit_count == 1) {
                goto done;
            }
            if (level->slot != 0) {
                level->slot = level->slot - 1;
            } else {
                level->slot = level->unit_count - 1;
            }
            direction = 2;
        }
        g_wldcore_thread8_params.redraw_request = direction;
        wldcore_unit_build_status_panel_data(g_wldcore_list_entry_values[level->slot] & 0x7FF);
        g_wldcore_threadc_params.redraw_request = 1;
        g_wldcore_thread9_params.redraw_request = 1;
    done:
        wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
    } else if (level->mode == 5) {
        if (world_thread_is_running(8) != 0) {
            return;
        }
        if (world_thread_is_running(0xC) != 0) {
            return;
        }
        if (world_thread_is_running(9) != 0) {
            return;
        }
        level->mode = 0;
        g_main_system_flags &= 0xFFFDF7FF;
        world_thread_start(0xE, world_text_message_box_thread);
        world_thread_set_parameters(0xE, 0x19, 0xB812, 0);
        world_thread_start(0xC, world_build_at_list_2);
        world_thread_set_parameters(0xC, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        g_wldcore_window_records[level->list_window].flags ^= 0x10;
        g_wldcore_window_render_records[level->layout_render].flags ^= 0x10;
        g_wldcore_window_records[level->frame_window_a].flags ^= 0x10;
        g_wldcore_window_records[level->frame_window_b].flags ^= 0x10;
        g_wldcore_window_render_records[level->preview_render].flags ^= 0x10;
        g_wldcore_window_render_records[level->list_render].flags ^= 0x10;
    }
}

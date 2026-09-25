#include "fft/wldcore.h"
#include "psx/pad.h"

/* Input handler for a layout-window menu level.
 *
 * Circle pops the level, X opens the row's help message, and a direction moves
 * to the neighbouring layout named by the row. A row with a counter pointer
 * adds the counter to the base message id, and suppresses the message entirely
 * when the counter reads 0 or -1 (except for layout 0x0b). */
void wldcore_menu_handle_layout_window_input(wldcore_window_layout_request_t* request) {
    s32 layout;
    u16* counter;
    s32 message;
    wldcore_window_record_t* record;
    s32 window;

    if ((g_wldcore_new_button_presses & PSX_PAD_CROSS) != 0) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
        g_wldcore_menu_ordering_table_offset = request->saved_state;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        return;
    }

    if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        layout = request->layout;
        counter = g_wldcore_window_layout_rows[layout].counter;
        if (counter != 0) {
            if (layout != 0xB && (u16)(*counter + 1) < 2) {
                return;
            }
            message = g_wldcore_window_layout_rows[layout].message_base + *counter;
        } else {
            message = g_wldcore_window_layout_rows[layout].message_base;
        }
        wldcore_menu_push_message_level((s16)message, 1);
        g_world_thread_task_active = 1;
        window = request->window_index;
        record = &g_wldcore_window_records[window];
        record->sequence = g_wldcore_window_records[window].sequence - 1;
        g_wldcore_window_records[window].palette = 0xA;
        record->anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        return;
    }

    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].up != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].up;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].down != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].down;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_RIGHT) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].left != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].left;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_LEFT) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].right != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].right;
            wldcore_window_apply_layout(request);
        }
    }
}

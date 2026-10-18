/* The per-frame event input update, with edge and auto-repeat input, a yield and the status-panel backdrops. */

#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Named fields hoist the address; keep the 32-bit event-input alias for scalar accesses. */
extern menu_input_state_t g_unit_list_input_event_state;

/* Per-frame event input update: derives the edge/auto-repeat input 32-bit value,
 * publishes it, and yields once while tracking thread status changes.
 *
 * Latches `frame_arg`, clears g_unit_panel_hide_numeric_values, and draws the
 * status-panel backdrops after the yield. Needs the _divcheck profile for the
 * three repeat-period divisions.
 */
void menu_input_update_event_state(u32* otag, u32 input, s32 frame_arg) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 previous_input;
    u32 counter;
    s32 thread_id;
    s32 status;

    g_gfx_current_otag_entry = otag;
    g_frame_arg = frame_arg;
    g_menu_input_controller = script_get_controller_input_pointer(0);
    g_unit_panel_hide_numeric_values = 0;
    if (input == 0) {
        g_input_initial_repeat_counter = 0;
        g_input_repeat_counter = 0;
        g_input_secondary_repeat_counter = 0;
    }

    previous_input = g_input_current;
    g_input_current = input;
    g_input_event = 0;
    g_previous_input = previous_input;

    if (previous_input != input
        || (filtered_input
               = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT))
            == 0) {
        g_input_event = ~previous_input & input;
        g_input_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_input_initial_repeat_counter;
        if (counter == initial_delay) {
            g_input_event = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_input_repeat_counter;
            g_input_secondary_repeat_counter++;
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_input_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_input_event = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_input_event = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_delay = g_input_secondary_repeat_period / g_script_event_speed;
            if (g_input_secondary_repeat_counter >= secondary_delay) {
                g_input_secondary_repeat_counter = 0;
                if ((g_input_current & PSX_PAD_SQUARE) != 0) {
                    g_input_event |= g_input_current & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_input_event |= g_input_current & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_input_repeat_counter = 0;
            g_input_secondary_repeat_counter = 0;
        }
    }

    g_sound_pending_sfx_id = -1;
    script_load_event_input_state(&g_unit_list_input_event_state);
    if (g_input_event != PSX_PAD_SELECT && g_input_event != 0) {
        g_script_input_suppression_frames = 5;
    }

    thread_id = 1;
    do {
        g_thread_status_snapshot[thread_id] = thread_is_running_asm(thread_id);
        thread_id++;
    } while (thread_id < 16);

    thread_yield();
    thread_id = 1;
    do {
        status = g_thread_status_snapshot[thread_id];
        if (status != thread_is_running_asm(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
        thread_id++;
    } while (thread_id < 16);

    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    menu_draw_status_panel_backdrops();
    g_event_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
}

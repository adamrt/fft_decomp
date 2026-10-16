/* The per-frame event input update, with edge and auto-repeat input and a yield. */

#include "fft/battle_event.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame event input update: derives the edge/auto-repeat input 32-bit value,
 * publishes it, and yields once while tracking thread status changes.
 *
 * CARD's body, without the other bodies' frame_arg latch,
 * the numeric-value flag and the thread status indicators; CARD instead sets
 * g_main_gfx_screen_polarity when `count_frame` is nonzero. A held direction
 * or L1/R1 repeats after g_main_input_repeat_initial_delay frames and then
 * every repeat period (directions) or secondary repeat period (L1/R1), all
 * divided by the event speed; holding Square restricts the fast repeat to
 * Left/Right and adds Up/Down to the slow one. Any input other than Select, or a thread starting or stopping across
 * the yield, sets five frames of event input suppression.
 */
void menu_input_update_event_state(u32* otag, u32 input, s32 count_frame) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 previous_input;
    u32 counter;
    s32 thread_id;
    s32 status;

    if (count_frame != 0) {
        g_main_gfx_screen_polarity = 1;
    }
    g_gfx_current_otag_entry = otag;
    g_input_controller = script_get_controller_input_pointer(0);
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
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_input_initial_repeat_counter;
        if (counter == initial_delay) {
            g_input_event = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_input_repeat_counter;
            g_input_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_input_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_input_event = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_input_event = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
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
    /* CARD inserts its controller pointer at +4; the copy of six 32-bit values includes it. */
    script_load_event_input_state((const menu_input_state_t*)&g_input_event);
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
    g_battle_event_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
}

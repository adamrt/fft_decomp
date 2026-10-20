/* WORLD publishes menu input to the event interpreter around one scheduler yield. Thread changes suppress script input
 * for the following frames. */

#include "fft/input.h"
#include "fft/formation.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* frame input state */
extern menu_input_state_t g_screen_input_frame_state;
static u32 g_screen_input_frame_new;
static u32 g_screen_input_frame_current;
static u32 g_screen_input_frame_previous;
static u32 g_screen_input_frame_initial_repeat_counter;
static u32 g_screen_input_frame_repeat_counter;
static u32 g_screen_input_frame_secondary_repeat_counter;

/* Per-frame event input update: derives the edge/auto-repeat 32-bit input mask,
 * publishes it, and yields once while tracking thread status changes. A new
 * press (other than Select alone) or any thread starting or stopping across
 * the yield suppresses script input for 5 frames.
 *
 * WORLD's body; publishes the state twice.
 * The six g_screen_input_frame_* values are laid out as a menu_input_state_t
 * (g_screen_input_frame_state), but a struct global makes GCC keep a base
 * register for the field accesses, so they are separate scalars here.
 */
void menu_input_event_state_update(u32* otag, u32 input, s32 frame_arg) {
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
    g_formation_screen_active = 0;
    if (input == 0) {
        g_screen_input_frame_initial_repeat_counter = 0;
        g_screen_input_frame_repeat_counter = 0;
        g_screen_input_frame_secondary_repeat_counter = 0;
    }

    previous_input = g_screen_input_frame_current;
    g_screen_input_frame_current = input;
    g_screen_input_frame_new = 0;
    g_screen_input_frame_previous = previous_input;

    /* Held D-pad, L1 and R1 auto-repeat once the initial delay passes: the
     * D-pad (only Left/Right while Square is held) every repeat period, and
     * L1/R1 (with Up/Down while Square is held, paging lists) every secondary
     * period. All delays are in frames divided by the event speed. */
    if (previous_input != input || (filtered_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_screen_input_frame_new = ~previous_input & input;
        g_screen_input_frame_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_screen_input_frame_initial_repeat_counter;
        if (counter == initial_delay) {
            g_screen_input_frame_new = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_screen_input_frame_repeat_counter;
            g_screen_input_frame_secondary_repeat_counter++;
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_screen_input_frame_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_screen_input_frame_new = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_screen_input_frame_new = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_input_secondary_repeat_period / g_script_event_speed;
            if (g_screen_input_frame_secondary_repeat_counter >= secondary_delay) {
                g_screen_input_frame_secondary_repeat_counter = 0;
                if ((g_screen_input_frame_current & PSX_PAD_SQUARE) != 0) {
                    g_screen_input_frame_new
                        |= g_screen_input_frame_current & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_screen_input_frame_new |= g_screen_input_frame_current & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_screen_input_frame_repeat_counter = 0;
            g_screen_input_frame_secondary_repeat_counter = 0;
        }
    }

    g_sound_pending_sfx_id = -1;
    script_load_event_input_state(&g_screen_input_frame_state);
    script_load_event_input_state(&g_screen_input_frame_state);
    if (g_screen_input_frame_new != PSX_PAD_SELECT && g_screen_input_frame_new != 0) {
        g_script_input_suppression_frames = 5;
    }

    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        g_thread_status_snapshot[thread_id] = thread_is_running_asm(thread_id);
    }

    thread_yield();
    for (thread_id = 1; thread_id < THREAD_ID_COUNT; thread_id++) {
        status = g_thread_status_snapshot[thread_id];
        if (status != thread_is_running_asm(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
    }

    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    menu_draw_status_panel_backdrops();
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
}

/* Return whether a thread slot is running. */
s32 menu_thread_running_check(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    return thread->is_running;
}

/* Event input state updates and the thread running check. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame event input update: derives the edge/auto-repeat 32-bit input mask,
 * publishes it, and yields once while tracking thread status changes.
 *
 * WORLD twin of bunit_input_update_event_state; publishes the state twice.
 * The six g_world_frame_* 32-bit values at 0x801c0528 are laid out as a
 * menu_input_state_t, but a struct global makes GCC keep a base
 * register for the field accesses, so they are separate scalars here.
 */
void menu_input_update_event_state_variant(u32* otag, u32 input, s32 frame_arg) {
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
        g_world_input_frame_initial_repeat_counter = 0;
        g_world_input_frame_repeat_counter = 0;
        g_world_input_frame_secondary_repeat_counter = 0;
    }

    previous_input = g_world_input_frame_current;
    g_world_input_frame_current = input;
    g_world_input_frame_new = 0;
    g_world_input_frame_previous = previous_input;

    if (previous_input != input || (filtered_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_world_input_frame_new = ~previous_input & input;
        g_world_input_frame_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_world_input_frame_initial_repeat_counter;
        if (counter == initial_delay) {
            g_world_input_frame_new = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_world_input_frame_repeat_counter;
            g_world_input_frame_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_world_input_frame_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_world_input_frame_new = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_world_input_frame_new = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_script_event_speed;
            if (g_world_input_frame_secondary_repeat_counter >= secondary_delay) {
                g_world_input_frame_secondary_repeat_counter = 0;
                if ((g_world_input_frame_current & PSX_PAD_SQUARE) != 0) {
                    g_world_input_frame_new
                        |= g_world_input_frame_current & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_world_input_frame_new |= g_world_input_frame_current & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_world_input_frame_repeat_counter = 0;
            g_world_input_frame_secondary_repeat_counter = 0;
        }
    }

    g_sound_pending_sfx_id = -1;
    script_load_event_input_state(&g_world_input_frame_state);
    script_load_event_input_state(&g_world_input_frame_state);
    if (g_world_input_frame_new != PSX_PAD_SELECT && g_world_input_frame_new != 0) {
        g_script_input_suppression_frames = 5;
    }

    for (thread_id = 1; thread_id < 16; thread_id++) {
        g_thread_status_snapshot[thread_id] = thread_is_running_asm_variant(thread_id);
    }

    thread_yield();
    for (thread_id = 1; thread_id < 16; thread_id++) {
        status = g_thread_status_snapshot[thread_id];
        if (status != thread_is_running_asm_variant(thread_id)) {
            g_script_input_suppression_frames = 5;
        }
    }

    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    menu_draw_thread_status_indicators_variant();
    g_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
}

s32 menu_thread_is_running_variant(s32 thread_id) {
    thread_t* thread = &g_threads[thread_id];

    return thread->is_running;
}

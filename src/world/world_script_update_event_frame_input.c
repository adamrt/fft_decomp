#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame event input update: derives the edge/auto-repeat input word,
 * publishes it, and yields once while tracking thread status changes.
 *
 * WORLD twin of bunit_input_update_event_state; publishes the state twice.
 * The six g_world_frame_* words at 0x801c0528 are laid out as a
 * world_menu_input_state_t, but a struct global makes GCC keep a base
 * register for the field accesses, so they are separate scalars here.
 */
void world_script_update_event_frame_input(u32* otag, u32 input, s16 frame_arg) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_delay;
    u32 previous_input;
    u32 counter;
    s32 thread_id;
    s32 status;

    g_world_gfx_active_otag_entries = otag;
    g_world_frame_arg = frame_arg;
    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    g_world_formation_screen_active = 0;
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
        initial_delay = g_main_input_repeat_initial_delay / g_world_event_speed;
        counter = ++g_world_input_frame_initial_repeat_counter;
        if (counter == initial_delay) {
            g_world_input_frame_new = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_world_input_frame_repeat_counter;
            g_world_input_frame_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_world_event_speed;
            if (counter >= repeat_delay) {
                g_world_input_frame_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_world_input_frame_new = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_world_input_frame_new = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_world_event_speed;
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

    g_world_sound_effect_id_to_play = -1;
    world_input_store_menu_state((const world_menu_input_state_t*)&g_world_input_frame_new);
    world_input_store_menu_state((const world_menu_input_state_t*)&g_world_input_frame_new);
    if (g_world_input_frame_new != PSX_PAD_SELECT && g_world_input_frame_new != 0) {
        g_world_thread_change_cooldown = 5;
    }

    thread_id = 1;
    do {
        g_world_thread_status_snapshot[thread_id] = world_thread_is_running_80100164(thread_id);
        thread_id++;
    } while (thread_id < 16);

    world_thread_yield();
    thread_id = 1;
    do {
        status = g_world_thread_status_snapshot[thread_id];
        if (status != world_thread_is_running_80100164(thread_id)) {
            g_world_thread_change_cooldown = 5;
        }
        thread_id++;
    } while (thread_id < 16);

    if (g_world_thread_change_cooldown != 0) {
        g_world_thread_change_cooldown--;
    }
    world_menu_draw_thread_status_indicators();
    g_world_frame_counter++;
    g_world_menu_packet_buffer_index = (g_world_menu_packet_buffer_index + 1) & 1;
    if (g_world_menu_input_block_frames != 0) {
        g_world_menu_input_block_frames--;
    }
}

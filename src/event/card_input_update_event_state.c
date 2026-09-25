#include "fft/event_card.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u16 g_battle_menu_input_block_frames;

void card_input_update_event_state(void* state, u32 input, s32 count_frame) {
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
    g_current_otag_entry = (u32*)state;
    g_card_input_controller = battle_script_get_controller_input_pointer(0);
    if (input == 0) {
        g_card_input_initial_repeat_counter = 0;
        g_card_input_repeat_counter = 0;
        g_card_input_secondary_repeat_counter = 0;
    }

    previous_input = g_card_input_current;
    g_card_input_current = input;
    g_card_input_event = 0;
    g_card_previous_input = previous_input;

    if (previous_input != input
        || (filtered_input
               = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT))
            == 0) {
        g_card_input_event = ~previous_input & input;
        g_card_input_initial_repeat_counter = 0;
    } else {
        initial_delay = g_main_input_repeat_initial_delay / g_battle_event_speed;
        counter = ++g_card_input_initial_repeat_counter;
        if (counter == initial_delay) {
            g_card_input_event = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_card_input_repeat_counter;
            g_card_input_secondary_repeat_counter++;
            repeat_delay = g_main_input_repeat_period / g_battle_event_speed;
            if (counter >= repeat_delay) {
                g_card_input_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_card_input_event = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_card_input_event = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_delay = g_main_input_secondary_repeat_period / g_battle_event_speed;
            if (g_card_input_secondary_repeat_counter >= secondary_delay) {
                g_card_input_secondary_repeat_counter = 0;
                if ((g_card_input_current & PSX_PAD_SQUARE) != 0) {
                    g_card_input_event |= g_card_input_current & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_card_input_event |= g_card_input_current & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_card_input_repeat_counter = 0;
            g_card_input_secondary_repeat_counter = 0;
        }
    }

    g_sound_effect_id_to_play = -1;
    battle_script_load_event_input_state(&g_card_input_event);
    if (g_card_input_event != PSX_PAD_SELECT && g_card_input_event != 0) {
        g_event_input_suppression_frames = 5;
    }

    thread_id = 1;
    do {
        g_card_thread_status_snapshot[thread_id] = battle_thread_is_running_8014cc94(thread_id);
        thread_id++;
    } while (thread_id < 16);

    battle_thread_yield();
    thread_id = 1;
    do {
        status = g_card_thread_status_snapshot[thread_id];
        if (status != battle_thread_is_running_8014cc94(thread_id)) {
            g_event_input_suppression_frames = 5;
        }
        thread_id++;
    } while (thread_id < 16);

    if (g_event_input_suppression_frames != 0) {
        g_event_input_suppression_frames--;
    }
    g_battle_event_frame_counter++;
    g_battle_menu_packet_buffer_index = (g_battle_menu_packet_buffer_index + 1) & 1;
    if (g_battle_menu_input_block_frames != 0) {
        g_battle_menu_input_block_frames--;
    }
}

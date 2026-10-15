#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Read modified up/down input with an initial delay and periodic repetition.
 *
 * Return -1 for up, 1 for down, 2 while waiting for a repeat, and 0 otherwise.
 * Both directions require input bit 0x80; timing scales with the WORLD rate.
 */
menu_vertical_scroll_direction_e menu_input_read_page_scroll_direction_variant(void) {
    s32 result = 0;
    s32 value;
    s32 delay;
    s32 repeat;
    u16 counter;

    if (g_world_menu_prompt_active == 0 && g_menu_input_lock_mode == 0 && g_world_script_tutorial_id == 0) {
        value = menu_input_read_tutorial_or_controller(0);
        if ((value & PSX_PAD_UP) && (value & PSX_PAD_SQUARE)) {
            g_world_menu_scroll_hold_counter++;
            result = -1;
        } else if ((value & PSX_PAD_DOWN) && (value & PSX_PAD_SQUARE)) {
            g_world_menu_scroll_hold_counter++;
            result = 1;
        } else {
            g_world_menu_scroll_hold_counter = 0;
        }
        value = menu_get_event_speed();
        delay = g_main_input_repeat_initial_delay / value;
        repeat = g_main_input_secondary_repeat_period / value;
        counter = g_world_menu_scroll_hold_counter;
        if (counter >= delay) {
            if (counter % repeat != 0) {
                return 2;
            }
        }
        counter = g_world_menu_scroll_hold_counter;
        if (counter >= 2) {
            if (counter < delay) {
                return 2;
            }
        }
    }
    return result;
}

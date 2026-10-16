/* The draw color and page scroll input. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "psx/pad.h"
#include "psx/types.h"

rgb24_t* menu_gfx_get_draw_color_variant(void) {
    return &g_world_menu_sprite_color;
}

/* Move the list viewport by one visible page (dir -1 up, 1 down), keeping the selected row's
 * offset within the page and clamping it on a short last page. WORLD's body reloads the list
 * layout and plays the cursor sound when the page moves. */
void menu_scroll_list_by_page_variant(menu_vertical_scroll_direction_e dir, const menu_instruction_t* menu) {
    s32 old;
    s16 delta;
    s32 unused[2]; /* 8 frame bytes no menu uses */

    old = g_menu_scroll_base_index;
    delta = g_menu_list_selected_index - g_menu_scroll_base_index;
    if (dir == -1) {
        g_menu_scroll_base_index -= menu->list.visible_rows;
        if (g_menu_scroll_base_index < 0) {
            g_menu_scroll_base_index = 0;
        }
    } else if (dir == 1) {
        g_menu_scroll_base_index += menu->list.visible_rows;
        /* `dir` is 1 here. */
        if (g_menu_list_entry_count - (g_menu_scroll_base_index + dir) < menu->list.visible_rows) {
            g_menu_scroll_base_index = g_menu_list_entry_count - menu->list.visible_rows;
        }
    }
    g_menu_list_selected_index = g_menu_scroll_base_index;
    world_menu_load_scrollable_list_layout(menu);
    g_menu_list_selected_index = delta + g_menu_scroll_base_index;
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (old != g_menu_scroll_base_index) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}

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
        value = world_menu_input_read_tutorial_or_controller(0);
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
                return MENU_VERTICAL_SCROLL_WAIT;
            }
        }
        counter = g_world_menu_scroll_hold_counter;
        if (counter >= 2) {
            if (counter < delay) {
                return MENU_VERTICAL_SCROLL_WAIT;
            }
        }
    }
    return result;
}

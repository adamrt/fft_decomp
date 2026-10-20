/* The draw color and page scroll input. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/tutorial.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u16 g_screen_scroll_hold_counter;

/* Return the sprite modulation color menus draw with (set by screen_palette_mode_set). */
rgb24_t* screen_get_draw_color(void) {
    return &g_screen_sprite_color;
}

/* Move the list viewport by one visible page (dir -1 up, 1 down), keeping the selected row's
 * offset within the page and clamping it on a short last page. WORLD's body reloads the list
 * layout and plays the cursor sound when the page moves. */
void screen_scroll_list_page(menu_vertical_scroll_direction_e dir, const menu_instruction_t* menu) {
    s32 old;
    s16 delta;
    s32 unused[2]; /* 8 frame bytes no menu uses */

    old = g_menu_scroll_base_index;
    delta = g_menu_list_selected_index - g_menu_scroll_base_index;
    if (dir == MENU_VERTICAL_SCROLL_UP) {
        g_menu_scroll_base_index -= menu->list.visible_rows;
        if (g_menu_scroll_base_index < 0) {
            g_menu_scroll_base_index = 0;
        }
    } else if (dir == MENU_VERTICAL_SCROLL_DOWN) {
        g_menu_scroll_base_index += menu->list.visible_rows;
        /* `dir` is 1 here; spelling the 1 through it matches the original. */
        if (g_menu_list_entry_count - (g_menu_scroll_base_index + dir) < menu->list.visible_rows) {
            g_menu_scroll_base_index = g_menu_list_entry_count - menu->list.visible_rows;
        }
    }
    g_menu_list_selected_index = g_menu_scroll_base_index;
    screen_list_layout_load(menu);
    g_menu_list_selected_index = delta + g_menu_scroll_base_index;
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (old != g_menu_scroll_base_index) {
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
}

/*
 * Read the page-scroll input (Square held with Up or Down) with an initial
 * delay and periodic repetition, both divided by the menu's frame step.
 *
 * Return UP or DOWN on the first frame and on each repeat, WAIT while the
 * delay or a repeat period runs, and NONE otherwise. Ignored (NONE) while a
 * prompt is open, input is locked or a tutorial runs.
 */
menu_vertical_scroll_direction_e menu_input_page_scroll_direction_read(void) {
    s32 result = MENU_VERTICAL_SCROLL_NONE;
    s32 value;
    s32 delay;
    s32 repeat;
    u16 counter;

    if (g_screen_prompt_active == 0 && g_menu_input_lock_mode == 0 && g_tutorial_id == 0) {
        value = screen_buttons_read(0);
        if ((value & PSX_PAD_UP) && (value & PSX_PAD_SQUARE)) {
            g_screen_scroll_hold_counter++;
            result = MENU_VERTICAL_SCROLL_UP;
        } else if ((value & PSX_PAD_DOWN) && (value & PSX_PAD_SQUARE)) {
            g_screen_scroll_hold_counter++;
            result = MENU_VERTICAL_SCROLL_DOWN;
        } else {
            g_screen_scroll_hold_counter = 0;
        }
        value = menu_get_event_speed();
        delay = g_input_repeat_initial_delay / value;
        repeat = g_input_secondary_repeat_period / value;
        counter = g_screen_scroll_hold_counter;
        if (counter >= delay && counter % repeat != 0) {
            return MENU_VERTICAL_SCROLL_WAIT;
        }
        counter = g_screen_scroll_hold_counter;
        if (counter >= 2 && counter < delay) {
            return MENU_VERTICAL_SCROLL_WAIT;
        }
    }
    return result;
}

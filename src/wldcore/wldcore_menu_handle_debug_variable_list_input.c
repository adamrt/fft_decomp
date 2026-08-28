#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level);

/* Input handler for the debug script-variable list level (type 0xb).
 *
 * Circle opens the number panel for the selected variable, X pops the level.
 * Up/down move the cursor, or scroll the first displayed variable by 10 with
 * L1 held and by 100 with R1 held. Scrolling redraws the rows through
 * wldcore_menu_render_debug_variable_list and clamps the cursor; a plain cursor move only repositions
 * the cursor window.
 *
 * The gotos are the target's layout: every scroll path joins one shared
 * store/redraw/clamp tail and every cursor path one move_cursor tail, which
 * rereads the cursor from memory. Structured arms with duplicated tails do
 * not cross-jump back into that shape. */
void wldcore_menu_handle_debug_variable_list_input(wldcore_menu_variable_list_level_t* level) {
    s32 variable;
    s32 value;
    s32 window;
    s32 dead[2];

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->window_index].palette = 0xA;
        g_wldcore_window_render_records[level->render_index].palette = 2;
        g_wldcore_window_records[level->window_index].sequence = 1;
        window = level->window_index;
        g_wldcore_window_records[window].frame_index = 0;
        g_wldcore_window_records[window].anim_counter = 0;
        variable = level->first_variable + level->cursor;
        wldcore_menu_push_number_panel_level_with_argument(variable, world_script_get_variable(variable));
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (g_wldcore_current_button_input & PSX_PAD_TRIANGLE) {
            if (level->first_variable < 0x64) {
                level->first_variable = 0;
            } else {
                level->first_variable = level->first_variable - 0x64;
            }
            goto redraw;
        }
        if (g_wldcore_current_button_input & PSX_PAD_SQUARE) {
            if (level->first_variable == 0) {
                value = 0x3fc;
            } else {
                value = level->first_variable - 0xa;
            }
            goto set_first;
        }
        if (level->cursor == 0) {
            level->cursor = level->row_count - 1;
        } else {
            level->cursor = level->cursor - 1;
        }
        goto move_cursor;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) == 0) {
        return;
    }
    if (g_wldcore_current_button_input & PSX_PAD_TRIANGLE) {
        if (level->first_variable + 0x64 < 0x3fd) {
            level->first_variable = level->first_variable + 0x64;
            goto redraw;
        }
        value = 0x3fc;
        goto set_first;
    }
    if (g_wldcore_current_button_input & PSX_PAD_SQUARE) {
        if (level->first_variable == 0x3fc) {
            level->first_variable = 0;
            goto redraw;
        }
        value = level->first_variable + 0xa;
        goto set_first;
    }
    goto cursor_down;
set_first:
    level->first_variable = value;
redraw:
    wldcore_menu_render_debug_variable_list(level);
    if (level->cursor < level->row_count) {
        return;
    }
    level->cursor = level->row_count - 1;
    goto move_cursor;
cursor_down:
    if (level->cursor == level->row_count - 1) {
        level->cursor = 0;
    } else {
        level->cursor = level->cursor + 1;
    }
move_cursor:
    g_wldcore_window_records[level->window_index].x = g_wldcore_window_render_records[level->render_index].base_x + 8;
    g_wldcore_window_records[level->window_index].y
        = g_wldcore_window_render_records[level->render_index].base_y - -(level->cursor * 0x10 + 0xC);
}

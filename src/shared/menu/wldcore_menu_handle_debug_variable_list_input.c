#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Input handler for the debug script-variable list level (type 0xb).
 *
 * Circle opens the number panel for the selected variable, X pops the level.
 * Up/down move the cursor, or scroll the first displayed variable by 10 with
 * L1 held and by 100 with R1 held. Scrolling redraws the rows through
 * wldcore_menu_render_debug_variable_list and clamps the cursor; a plain cursor move only repositions
 * the cursor window.
 *
 * Every scroll arm falls into the one redraw, which clamps the cursor and
 * joins the cursor move. The cursor-down arm sits between the redraw and the
 * move in the target, so it and the move are reached by goto: a redraw
 * repeated in each arm is not merged back (its clamp store is cross-jumped
 * into the cursor-down arm first). */
void wldcore_menu_handle_debug_variable_list_input(wldcore_menu_variable_list_level_t* level) {
    s32 variable;
    s32 dead[2];

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->window_index].palette = 0xA;
        g_wldcore_window_render_records[level->render_index].palette = 2;
        g_wldcore_window_records[level->window_index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        g_wldcore_window_records[level->window_index].anim_counter
            = g_wldcore_window_records[level->window_index].frame_index = 0;
        variable = level->first_variable + level->cursor;
        wldcore_menu_push_number_panel_level_with_argument(variable, script_get_variable(variable));
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->first_variable < 0x64) {
                level->first_variable = 0;
            } else {
                level->first_variable -= 0x64;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->first_variable == 0) {
                level->first_variable = 0x3fc;
            } else {
                level->first_variable -= 0xa;
            }
        } else {
            if (level->cursor == 0) {
                level->cursor = level->row_count - 1;
            } else {
                level->cursor--;
            }
            goto move_cursor;
        }
    } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->first_variable + 0x64 < 0x3fd) {
                level->first_variable += 0x64;
            } else {
                level->first_variable = 0x3fc;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->first_variable == 0x3fc) {
                level->first_variable = 0;
            } else {
                level->first_variable += 0xa;
            }
        } else {
            goto cursor_down;
        }
    } else {
        return;
    }
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
        level->cursor++;
    }
move_cursor:
    g_wldcore_window_records[level->window_index].position.x
        = g_wldcore_window_render_records[level->render_index].base.x + 8;
    g_wldcore_window_records[level->window_index].position.y
        = g_wldcore_window_render_records[level->render_index].base.y - -(level->cursor * 0x10 + 0xC);
}

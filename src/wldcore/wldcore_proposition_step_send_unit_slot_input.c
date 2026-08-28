#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_window_load_party_unit_sprite_record(wldcore_menu_send_unit_level_t* level, s32 unit, s32 render_index);
void world_thread_request_redraw(s32 thread_id);
u32 wldcore_input_check_repeating_directional(u32 buttons);

/* Input step of the proposition send-unit level.
 *
 * While the portrait frame is visible, a changed unit in the selected slot is
 * redrawn into the portrait. Cancel (0x40) and confirm (0x20) both close the
 * level; confirm first stores the highlighted list row into the slot, redraws
 * the rows, rebuilds the available-unit list and flags WORLD thread 12. Up and
 * down wrap the slot through 0..2 and move the cursor window.
 *
 * The target's frame holds two words no instruction touches, reproduced by
 * the unused point. The row offset is spelled as a subtraction of a negative
 * step so the record's base_y stays the first addend, as in wldcore_list_handle_active_propositions_input. */
void wldcore_proposition_step_send_unit_slot_input(wldcore_menu_send_unit_level_t* level) {
    wldcore_point32_t unused;
    u32 buttons;
    s32 unit;
    s32 window_a;
    s32 window_b;

    if ((g_wldcore_window_render_records[level->portrait_frame].flags & 0x100) == 0) {
        unit = g_wldcore_proposition_send_units[level->slot] & 0x7FF;
        if (unit != level->shown_unit) {
            level->shown_unit = unit;
            wldcore_window_load_party_unit_sprite_record(level, unit, level->portrait_render);
        }
    }
    buttons = g_wldcore_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: a duplicated close path cross-jumps to the same
         * layout, but without this label the scheduler reorders its loads. */
        goto close;
    }
    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        unit = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & 0x7FF;
        g_wldcore_proposition_send_units[level->slot] = unit;
        wldcore_window_draw_text_rows(level);
        wldcore_proposition_load_send_unit_candidates(level);
        world_thread_request_redraw(0xC);
    close:
        window_b = level->frame_window_b;
        window_a = level->frame_window_a;
        g_wldcore_window_records[window_b].palette = 0;
        g_wldcore_window_records[window_a].palette = 0;
        g_wldcore_window_render_records[level->portrait_render].flags |= 0x10;
        g_wldcore_window_render_records[level->portrait_frame].flags |= 0x10;
        g_wldcore_window_records[level->portrait_window].flags |= 0x10;
        g_wldcore_window_records[level->cursor_window].flags |= 0x10;
        g_wldcore_menu_result = 0x1E0;
        level->mode = 0;
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->slot == 0) {
            level->slot = 2;
        } else {
            level->slot = level->slot - 1;
        }
    } else if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (level->slot == 2) {
            level->slot = 0;
        } else {
            level->slot = level->slot + 1;
        }
    } else {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    g_wldcore_window_records[level->cursor_window].x = g_wldcore_window_render_records[level->layout_render].base_x + 6;
    g_wldcore_window_records[level->cursor_window].y
        = g_wldcore_window_render_records[level->layout_render].base_y - level->slot * -16 + 12;
}

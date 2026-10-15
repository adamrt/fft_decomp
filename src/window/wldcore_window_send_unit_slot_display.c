/* Send-unit slot rows, portrait record, slot input step and the unit panel thread start. */

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Redraw the send-unit layout render record: a window frame with one
 * 16-pixel row per chosen unit name. */
void wldcore_window_draw_text_rows(wldcore_menu_send_unit_level_t* level) {
    point32_t point;
    s32 index;
    s32 i;

    index = level->list_frame.render_index;
    world_menu_build_window_frame_image(g_wldcore_window_render_records[index].bounds.w,
        g_wldcore_window_render_records[index].bounds.h, (u16*)g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        window_display_text_entry_variant(
            index, g_wldcore_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

/* Point render record `index` at the formation portrait of party member
 * `party_index`: its texture page, cell rectangle and CLUT (16 colors at
 * x 0x280 + 0x40 per 40 portraits + 16 * (code % 4), y 0x1F0 +
 * (code % 40) / 4), at full brightness, and show the record. The first
 * argument is not read. */
void wldcore_window_load_party_unit_sprite_record(wldcore_menu_send_unit_level_t* unused, s32 party_index, s32 index) {
    POLY_FT4 poly;
    s32 code;
    s32 cell;
    CVECTOR* color;
    RECT* bounds;

    code = world_get_party_unit_formation_sprite(party_index);
    cell = code;
    menu_build_unit_portrait_poly_variant(&poly, code);
    g_wldcore_window_render_records[index].clut.x = (code / 40 << 6) + 0x280 + cell % 4 * 16;
    g_wldcore_window_render_records[index].clut.y = cell % 40 / 4 + 0x1F0;
    g_wldcore_window_render_records[index].tpage = poly.tpage;
    bounds = &g_wldcore_window_render_records[index].bounds;
    bounds->x = poly.u0;
    bounds->y = poly.v0;
    bounds->w = poly.u1 - poly.u0;
    bounds->h = poly.v2 - poly.v0;
    color = &g_wldcore_window_render_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    g_wldcore_window_render_records[index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
}

/* Input step of the proposition send-unit level.
 *
 * While the portrait frame is visible, a changed unit in the selected slot is
 * redrawn into the portrait. Cancel (0x40) and confirm (0x20) both close the
 * level; confirm first stores the highlighted list row into the slot, redraws
 * the rows, rebuilds the available-unit list and flags WORLD thread 12. Up and
 * down wrap the slot through 0..2 and move the cursor window.
 *
 * The target's frame holds two 32-bit values no instruction touches, reproduced by
 * the unused point. */
void wldcore_proposition_step_send_unit_slot_input(wldcore_menu_send_unit_level_t* level) {
    point32_t unused;
    u32 buttons;
    s32 unit;

    if ((g_wldcore_window_render_records[level->portrait_frame].flags & WORLD_RENDER_RECORD_POP_IN) == 0) {
        unit = g_wldcore_proposition_send_units[level->slot] & 0x7FF;
        if (unit != level->shown_unit) {
            level->shown_unit = unit;
            wldcore_window_load_party_unit_sprite_record(level, unit, level->portrait_render);
        }
    }
    buttons = g_wldcore_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: a duplicated close path cross-jumps to the same
         * layout, but without this label the scheduler reorders its loads. */
        goto close;
    }
    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        unit = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        g_wldcore_proposition_send_units[level->slot] = unit;
        wldcore_window_draw_text_rows(level);
        wldcore_proposition_load_send_unit_candidates(level);
        world_thread_request_redraw(WLDCORE_THREAD_SLOT_WINDOW_PANEL);
    close:
        g_wldcore_window_records[level->frame_window_a].palette
            = g_wldcore_window_records[level->frame_window_b].palette = 0;
        g_wldcore_window_render_records[level->portrait_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->portrait_frame].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->portrait_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->cursor_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        level->mode = WLDCORE_SEND_UNIT_MODE_SELECT;
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->slot == 0) {
            level->slot = 2;
        } else {
            level->slot--;
        }
    } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->slot == 2) {
            level->slot = 0;
        } else {
            level->slot++;
        }
    } else {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    g_wldcore_window_records[level->cursor_window].position.x
        = g_wldcore_window_render_records[level->list_frame.render_index].base.x + 6;
    g_wldcore_window_records[level->cursor_window].position.y
        = g_wldcore_window_render_records[level->list_frame.render_index].base.y + level->slot * 16 + 12;
}

/* Build the status panel data of party member `party_index` and start the
 * three unit panel threads on it (status banner, numeric editor, equipment
 * and ability panel), raising MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS. */
void wldcore_start_core_background_threads(s32 party_index) {
    menu_status_thread_t* thread8;

    wldcore_unit_build_status_panel_data(party_index);
    thread8 = &g_wldcore_thread8_params;
    g_wldcore_thread9_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread8_params.y = 0;
    thread8->x = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)thread8, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;
}

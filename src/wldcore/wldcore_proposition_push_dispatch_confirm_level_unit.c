#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Pushes menu level type 0xd: a cursor window, an upper window, a 0x28x0x30
 * text record (0xb805) and a text record listing text 0xb84e, the errand
 * name (0xf7ff + g_wldcore_selected_proposition_row[0].name_text), text 0xb821 and
 * g_wldcore_proposition_send_unit_count rows of text 0x4000 + g_wldcore_proposition_send_units[i]. World thread 14
 * shows message 0xb814; both windows are placed against the first text record.
 *
 * `unused` reproduces the target's 0x78-byte frame. The row offset uses its
 * own local: reusing `row` (then set twice) schedules the loop's `addiu v1`
 * ahead of `move a0`. */
void wldcore_proposition_push_dispatch_confirm_level(void) {
    RECT bounds;
    point32_t point;
    s16 size[2];
    s32 unused[8];
    point32_t* position;
    point32_t* base;
    u16* location;
    s32 index;
    s32 depth;
    s32 i;
    s32 row;
    s32 line;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CONFIRM_CONDITIONS, 0);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    position = &g_wldcore_window_render_records[index].base;
    position->x = 0x48;
    position->y = 0x20;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    /* The summary panel's render record index lands in the 32-bit row field, which is cleared below. */
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.row = index;
    location = &g_wldcore_selected_proposition_row[0].name_text;
    text_measure_pixels(&size[0], &size[1], text_find_entry(*location + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.x = 0;
    row = size[1] + 2;
    bounds.h = (g_wldcore_proposition_send_unit_count - -row) * 0x10 + 0x10;
    g_text_substitution_values[1] = g_wldcore_proposition_dispatch_days;
    g_text_substitution_values[0] = g_wldcore_proposition_dispatch_days;
    g_text_substitution_values[2] = g_wldcore_map_projection_state.marker.kind + TEXT_ID_LOCATION_NAME_BASE;
    wldcore_window_build_render_record_image(index, bounds, 0, &g_wldcore_window_image_buffer[0x200]);
    point.x = 8;
    point.y = size[1] * 0x10 + 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONDITION_LABELS, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 8;
    point.y = 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry_variant(
        index, *location + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, &g_wldcore_window_image_buffer[0x200]);
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        point.x = 0x3C;
        line = i + 2;
        point.y = (size[1] + line) * 0x10 + 8;
        window_display_text_entry_variant(index, g_wldcore_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point,
            &g_wldcore_window_image_buffer[0x200]);
    }
    wldcore_window_load_image_record_to_vram(index, &g_wldcore_window_image_buffer[0x200]);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;

    depth = g_wldcore_menu_stack_depth;
    index = g_wldcore_menu_stack_records_next[depth].choice_panel.render_index;
    g_wldcore_menu_stack_records_next[depth].choice_panel.row = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_menu_stack_records_next[depth].choice_panel.row * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM;
}

/* Input step of the proposition-dispatch yes/no panel built by wldcore_window_build_yes_no_panel:
 * up/down toggle the row, cancel (0x40) and "no" close the panel; "yes" pays
 * g_wldcore_proposition_gil_amount from script variable 0x2c, marks the proposition taken, records
 * it in the next g_main_active_propositions slot with its participants, and
 * closes the panel. */
void wldcore_proposition_handle_dispatch_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 row;
    s32 i;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and both confirm paths
         * jump into it; duplicated copies cross-jump into the last one. */
    block_tail:
        g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_FUNDS;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_list_open_available_propositions();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        row = level->row ^ 1;
        level->row = row;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->render_index].base.y - -(row * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        if (level->row != 1) {
            script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS,
                script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) - g_wldcore_proposition_gil_amount);
            g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
            script_set_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_get_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED);
            g_main_active_propositions[g_main_save_proposition_count].flags = WLDCORE_PROPOSITION_PROGRESS_DISPATCHED;
            g_main_active_propositions[g_main_save_proposition_count].proposition_id
                = g_wldcore_selected_proposition_row[0].id - 1;
            g_main_active_propositions[g_main_save_proposition_count].elapsed_days = 0;
            g_main_active_propositions[g_main_save_proposition_count].assigned_days
                = g_wldcore_proposition_dispatch_days;
            g_main_active_propositions[g_main_save_proposition_count].location
                = g_wldcore_map_projection_state.marker.kind;
            g_main_active_propositions[g_main_save_proposition_count].participant_count
                = g_wldcore_proposition_send_unit_count;
            for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
                wldcore_get_party_unit(g_wldcore_proposition_send_units[i])->proposition_status = 1;
                g_main_active_propositions[g_main_save_proposition_count].participant_indices[i]
                    = g_wldcore_proposition_send_units[i];
            }
            g_main_save_proposition_count++;
            script_set_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_get_variable(g_wldcore_selected_proposition_row[0].id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS);
        }
        goto block_tail;
    }
}

/*
 * Advance daily proposition progress and packed countdown bytes.
 *
 * Records without flag 0x04 increment their elapsed byte. Set flag 0x04 when
 * it equals the assigned duration or flag 0x02 is set. Nonzero low-six-bit
 * countdowns decrease from at most 32, retaining their high 2 bits.
 */
void wldcore_proposition_advance_daily_counters(void) {
    s32 i;
    u8* counter;

    for (i = 0; i < g_main_save_proposition_count; i++) {
        if (!(g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED)) {
            g_main_active_propositions[i].elapsed_days++;
            if (g_main_active_propositions[i].assigned_days == g_main_active_propositions[i].elapsed_days
                || (g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RECALLED)) {
                g_main_active_propositions[i].flags |= WLDCORE_PROPOSITION_PROGRESS_RETURNED;
            }
        }
    }
    counter = g_main_proposition_states;
    do {
        u32 value = *counter;
        s32 days = value & WLDCORE_PROPOSITION_STATE_DAYS_MASK;
        if (days != 0) {
            if (days > 32) {
                days = 32;
            }
            days--;
            *counter = (value & (WLDCORE_PROPOSITION_STATE_FAILURE | WLDCORE_PROPOSITION_STATE_BAD_FAILURE)) | days;
        }
        counter++;
    } while ((s32)counter < (s32)(g_main_proposition_states + 96)); /* Target uses signed SLT. */
}

/*
 * Count completed propositions at a location, or at every location.
 *
 * The daily updater sets completion flag 0x04. A location of -1 accepts every
 * completed record; the stored count is not clamped to the save-data capacity.
 */
s32 wldcore_proposition_count_completed(s32 location) {
    s32 i;
    s32 count = 0;
    for (i = 0; i < g_main_save_proposition_count; i++) {
        if (g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED) {
            if (location == -1 || g_main_active_propositions[i].location == location)
                count++;
        }
    }
    return count;
}

/* Scalar view of g_wldcore_window_panel_render_state.select_text_table: as a
 * struct field its address would share the selected_index base register, but
 * the target stores it by symbol. */
extern s16 g_wldcore_menu_scroll_offset;

/* Push the proposition send-unit menu level (type 0x24).
 *
 * Builds the available-unit list into the new level record, starts WORLD
 * thread 12 on the list-panel renderer with the shared panel render state,
 * appends the level's seven window records and five render records, and asks
 * thread 14 for message 0xb812.
 *
 * The selected index is written through the panel render state (+0x38):
 * the target derives the thread parameter from that field's address. Each
 * record's x/y pair is written through a block-local pointer so the pointer
 * shares the index product's register, and the chained frame_index/anim_counter
 * store keeps anim_counter's base in a register as in the target. */
void wldcore_proposition_push_send_unit_level(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    RECT bounds;
    s32 index;
    s32 depth;
    s32 count;

    g_wldcore_menu_scroll_offset = 0;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_proposition_send_unit_count = 0;
    count = wldcore_proposition_load_send_unit_candidates(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit);
    layout.text_width = 0x6E;
    layout.visible_rows = count;
    origin.x = 8;
    origin.y = 0x50;
    wldcore_window_init_panel_render_state(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.unit_count, &layout, &origin);
    g_wldcore_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_wldcore_window_panel_label_state.x[1] = 0x58;
    g_wldcore_window_panel_label_state.text_ids[1] = g_wldcore_proposition_candidate_levels;
    g_wldcore_window_panel_label_state.text_colors[1] = g_wldcore_list_row_flags;
    g_wldcore_window_panel_label_state.x[0] = 0;
    g_wldcore_window_panel_render_state.header_id = 0x10;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_menu_scroll_offset = 0;
    thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
    thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.window_index = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.render_index = index;
    bounds.x = 0x80;
    bounds.y = 0;
    bounds.w = 0x6C;
    bounds.h = 0x40;
    window_init_text_record_variant(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_a = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_A;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_b = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_B;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.preview_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_render = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_init_text_record_variant(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_init_text_record_variant(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_window = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = 0x3E;
        position->y = 0x28;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_PORTRAIT;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.left_arrow = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = -0x74;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.right_arrow = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = 0x60;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_UNITS, 0);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].send_unit.shown_preview = -1;
    g_wldcore_menu_stack_records_next[depth].send_unit.mode = WLDCORE_SEND_UNIT_MODE_SELECT;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Build the list of available non-guest human units for a proposition.
 *
 * Units already assigned to a proposition, absent roster entries, monsters,
 * and special-character sprite sets are excluded. */
s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level) {
    s32 i;
    s32 j;
    party_unit_t* unit;

    i = 0;
    level->unit_count = 0;
    for (; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_unit(i);
        if (unit->proposition_status != 0) {
            continue;
        }
        if (unit->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (unit->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
            continue;
        }
        for (j = 0; j < g_wldcore_proposition_send_unit_count; j++) {
            if (g_wldcore_proposition_send_units[j] == i) {
                break;
            }
        }
        if (j < g_wldcore_proposition_send_unit_count) {
            g_wldcore_list_row_flags[level->unit_count] = 8;
        } else {
            g_wldcore_list_row_flags[level->unit_count] = 0;
        }
        g_wldcore_list_entry_values[level->unit_count] = i + TEXT_ID_UNIT_NAME_BASE;
        g_wldcore_proposition_candidate_levels[level->unit_count] = unit->level;
        level->unit_count = level->unit_count + 1;
    }
    if (level->unit_count < 6) {
        i = level->unit_count;
    } else {
        i = 6;
    }
    return i;
}

/* Proposition dispatch confirmation, daily counters, the send unit level and browser, completed propositions, report
 * and result levels, and report speaker and message selection. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_gfx.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_menu_level.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_sprite.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void wldcore_menu_push_proposition_report_level(s32 argument);
static s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level);
static void wldcore_proposition_prepare_report(s32 proposition_index);
static void wldcore_proposition_push_result_level(s32 proposition);
static s32 wldcore_proposition_select_report_speaker(s32 index);
static s32 wldcore_proposition_step_message(wldcore_proposition_report_level_t* state, s32 message);

/* Pushes WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM, the summary of a job
 * about to be dispatched: a cursor window, a header window, the OK/NO record
 * and a summary record with "Job contents:" and the job's contents text
 * (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1 + name_text), the days and place
 * labels (WLDCORE_TEXT_ID_JOB_CONDITION_LABELS) and one row per chosen unit.
 * The message box asks WLDCORE_TEXT_ID_CONFIRM_CONDITIONS; both windows are
 * placed against the OK/NO record.
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

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    position = &g_wldcore_window_render_records[index].base;
    position->x = 0x48;
    position->y = 0x20;

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
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
    window_display_text_entry(
        index, WLDCORE_TEXT_ID_JOB_CONDITION_LABELS, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 8;
    point.y = 8;
    window_display_text_entry(index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry(
        index, *location + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, &g_wldcore_window_image_buffer[0x200]);
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        point.x = 0x3C;
        line = i + 2;
        point.y = (size[1] + line) * 0x10 + 8;
        window_display_text_entry(index, g_wldcore_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point,
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

/* Input step of the dispatch confirmation's OK/NO panel: up/down toggle the
 * row, Cross and NO close the panel; OK pays g_wldcore_proposition_gil_amount
 * from the war funds, marks the proposition attempted and in progress,
 * records it in the next g_main_active_propositions slot with its
 * participants (each marked busy), and closes the panel. Either way the
 * propositions list reopens. */
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
 * Every active proposition not yet returned counts one more elapsed day, and
 * returns (WLDCORE_PROPOSITION_PROGRESS_RETURNED) when that reaches its
 * assigned days or it was recalled. Each proposition's days until it is
 * offered again drop by one, from at most 32, keeping its failure bits.
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
    } while ((s32)counter < (s32)(g_main_proposition_states + EVENT_SCRIPT_VAR_PROPOSITION_COUNT)); /* signed slt */
}

/*
 * Count the active propositions that have returned
 * (WLDCORE_PROPOSITION_PROGRESS_RETURNED) to a location, or to any location
 * when `location` is -1.
 */
s32 wldcore_proposition_count_returned(s32 location) {
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

/* Push the send-unit level (WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT).
 *
 * Builds the available-unit list into the new level record, starts the panel
 * thread on it with each unit's level as a second column, appends the level's
 * seven window records and five render records, and asks
 * WLDCORE_TEXT_ID_CHOOSE_UNITS.
 *
 * The selected index is written through the panel render state:
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

    /* The initializer hides label column 1; show it with each candidate's level as digits. */
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

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.window_index = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.render_index = index;
    bounds.x = 0x80;
    bounds.y = 0;
    bounds.w = 0x6C;
    bounds.h = 0x40;
    window_init_text_record(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_a = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_A;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_b = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_B;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.preview_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_render = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_init_text_record(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_init_text_record(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
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

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
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

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
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
static s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level) {
    s32 i = 0;
    s32 j;
    party_unit_t* unit;

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

/* Redraw the send-unit list window: the empty frame, then one name row per unit chosen so far. */
static void wldcore_proposition_draw_send_unit_names(wldcore_menu_send_unit_level_t* level) {
    point32_t point;
    s32 index = level->list_frame.render_index;
    s32 i;

    world_menu_build_window_frame_image(g_wldcore_window_render_records[index].bounds.w,
        g_wldcore_window_render_records[index].bounds.h, (u16*)g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        window_display_text_entry(
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
static void wldcore_window_load_party_unit_sprite_record(
    wldcore_menu_send_unit_level_t* unused, s32 party_index, s32 index) {
    POLY_FT4 poly;
    s32 code;
    s32 cell;
    CVECTOR* color;
    RECT* bounds;

    code = world_get_party_unit_formation_sprite(party_index);
    cell = code;
    menu_build_unit_portrait_poly(&poly, code);
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
 * redrawn into the portrait. Cross and Circle both leave slot editing; Circle
 * first stores the highlighted list unit into the slot, redraws the names,
 * rebuilds the available-unit list and asks the panel thread to redraw. Up
 * and down wrap the slot through 0..2 and move the cursor window.
 *
 * The target's frame holds two 32-bit values no instruction touches, reproduced by
 * the unused point. */
static void wldcore_proposition_step_send_unit_slot_input(wldcore_menu_send_unit_level_t* level) {
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
        wldcore_proposition_draw_send_unit_names(level);
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
static void wldcore_unit_start_status_panel_threads(s32 party_index) {
    menu_status_thread_t* thread8;

    wldcore_unit_build_status_panel_data(party_index);
    thread8 = &g_wldcore_thread8_params;
    g_wldcore_thread9_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread8_params.y = 0;
    thread8->x = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)thread8, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;
}

/* Status view of the send-unit level: once the panel threads stop it opens the unit's status
 * screens; L1 and R1 page through the candidates with wraparound, Select opens the layout window,
 * Circle or Cross close the view and return to 'So, who you wanna send?'. */
static void wldcore_proposition_step_send_unit_browser(wldcore_menu_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;
    s16* menu_selected_index;

    if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_OPEN) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) != 0) {
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_wldcore_window_records[level->left_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->right_arrow].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        level->mode = WLDCORE_SEND_UNIT_MODE_STATUS;
        menu_selected_index = &g_wldcore_window_panel_render_state.selected_index;
        wldcore_unit_start_status_panel_threads(g_wldcore_list_entry_values[*menu_selected_index] & TEXT_ID_ENTRY_MASK);
        level->slot = *menu_selected_index;
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS) {
        buttons = g_wldcore_input_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_wldcore_window_records[level->left_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->right_arrow].flags |= WORLD_RENDER_RECORD_HIDDEN;
            level->mode = WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE;
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette
                = 0xA;
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            wldcore_menu_push_layout_window_level();
            return;
        }
        if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter
                = g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
        }
        if (wldcore_input_update_repeat(PSX_PAD_R1) != 0
            && g_wldcore_window_records[level->left_arrow].sequence != WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS) {
            g_wldcore_window_records[level->right_arrow].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS;
            g_wldcore_window_records[level->right_arrow].frame_index
                = g_wldcore_window_records[level->right_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                /* Shared tail: duplicating the sound call per arm is only
                 * partly cross-jumped and leaves an extra copy. */
                goto done;
            }
            selected = level->slot + 1;
            if (selected >= level->unit_count) {
                level->slot = 0;
            } else {
                level->slot = selected;
            }
            direction = 1;
        } else {
            if (wldcore_input_update_repeat(PSX_PAD_L1) == 0
                || g_wldcore_window_records[level->right_arrow].sequence == WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS) {
                return;
            }
            g_wldcore_window_records[level->left_arrow].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS;
            g_wldcore_window_records[level->left_arrow].frame_index
                = g_wldcore_window_records[level->left_arrow].anim_counter = 0;
            if (level->unit_count == 1) {
                goto done;
            }
            if (level->slot != 0) {
                level->slot--;
            } else {
                level->slot = level->unit_count - 1;
            }
            direction = 2;
        }
        g_wldcore_thread8_params.redraw_request = direction;
        wldcore_unit_build_status_panel_data(g_wldcore_list_entry_values[level->slot] & TEXT_ID_ENTRY_MASK);
        g_wldcore_threadc_params.redraw_request = 1;
        g_wldcore_thread9_params.redraw_request = 1;
    done:
        wldcore_sound_play_effect(MAIN_SFX_PAGE_SWITCH);
    } else if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_EDITOR) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        level->mode = WLDCORE_SEND_UNIT_MODE_SELECT;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS);
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_UNITS, 0);
        thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        g_wldcore_window_records[level->list_frame.window_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_frame.render_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_a].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_b].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->preview_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_render].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    }
}

/* Undo wldcore_window_set_states_and_blue_tint when a sub-level closes:
 * default palettes on the send-unit windows and a neutral preview portrait. */
void wldcore_menu_resume_proposition_send_unit_level(wldcore_menu_send_unit_level_t* level) {
    CVECTOR* color;

    g_wldcore_window_records[level->list_frame.window_index].palette
        = g_wldcore_window_render_records[level->list_frame.render_index].palette
        = g_wldcore_window_render_records[level->list_render].palette
        = g_wldcore_window_records[level->cursor_window].palette = 0;
    /* Each channel store reloads the level's render index. */
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->r = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->g = 0x80;
    color = &g_wldcore_window_render_records[level->preview_render].color;
    color->b = 0x80;
    g_wldcore_window_records[level->left_arrow].palette = g_wldcore_window_records[level->right_arrow].palette = 0;
}

/* Gray out the send-unit windows (palettes 6, 10 and 2) and tint the preview
 * portrait blue (0x40, 0x50, 0x70) while a sub-level runs over them; undone by
 * wldcore_window_clear_states_and_restore_tint. */
void wldcore_menu_suspend_proposition_send_unit_level(wldcore_menu_send_unit_level_t* level) {
    s32 pad[4];

    g_wldcore_window_records[level->list_frame.window_index].palette = 6;
    g_wldcore_window_render_records[level->list_frame.render_index].palette = 2;
    g_wldcore_window_render_records[level->list_render].palette = 2;
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_render_records[level->preview_render].color.r = 0x40;
    g_wldcore_window_render_records[level->preview_render].color.g = 0x50;
    g_wldcore_window_render_records[level->preview_render].color.b = 0x70;
}

/* Top-level step of the proposition send-unit menu level, dispatching on the
 * level's mode.
 *
 * CANCEL and ACCEPT wait for the panel thread to close and then pop the level,
 * ACCEPT continuing into the proposition detail level and its days panel; the
 * STATUS modes run wldcore_proposition_step_send_unit_browser and REPLACE runs
 * wldcore_proposition_step_send_unit_slot_input.
 *
 * Otherwise (SELECT) the highlighted unit's portrait is shown in the preview
 * record once the list has popped in, then the new button presses are
 * handled: Select opens help message 0x109a, Start (with at least one chosen
 * unit) accepts, Cross cancels, Square hides the list and opens the unit's
 * status panels, and Circle toggles the highlighted unit in or out of the
 * three g_wldcore_proposition_send_units slots (removing it compacts the
 * list; adding one when all three are full switches to REPLACE).
 *
 * The target reserves a 0x30-byte frame but only saves s0 and ra; the unused
 * 16-byte array reproduces that frame. The removal and addition paths each
 * end in their own redraw calls, which jump2 cross-jumps from the count
 * store onward. */
void wldcore_menu_step_send_unit_level(wldcore_menu_send_unit_level_t* level) {
    s32 buttons;
    s32 value;
    s32 i;
    s32 unused[4];

    if (level->mode == WLDCORE_SEND_UNIT_MODE_CANCEL) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_available_propositions();
        }
        return;
    }

    if (level->mode == WLDCORE_SEND_UNIT_MODE_ACCEPT) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_menu_stack_depth--;
            wldcore_proposition_push_detail_level(g_wldcore_selected_proposition_row[0].id - 1);
            wldcore_menu_suspend_proposition_detail_level(&level->list_frame);
            wldcore_menu_push_number_panel_level();
        }
        return;
    }

    if (level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_OPEN || level->mode == WLDCORE_SEND_UNIT_MODE_STATUS
        || level->mode == WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE) {
        wldcore_proposition_step_send_unit_browser(level);
        return;
    }

    if (level->mode == WLDCORE_SEND_UNIT_MODE_REPLACE) {
        wldcore_proposition_step_send_unit_slot_input(level);
        return;
    }

    if (!(g_wldcore_window_render_records[level->list_frame.render_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        if (value != level->shown_preview) {
            level->shown_preview = value;
            wldcore_window_load_party_unit_sprite_record(level, value, level->preview_render);
        }
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_SELECT) {
        wldcore_menu_suspend_proposition_send_unit_level(level);
        wldcore_menu_push_message_level(0x109A, 1);
        return;
    }

    if (buttons & PSX_PAD_START) {
        if (g_wldcore_proposition_send_unit_count == 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            return;
        }
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->mode = WLDCORE_SEND_UNIT_MODE_ACCEPT;
        g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS;
        g_wldcore_displayed_numeric_value_secondary = g_wldcore_proposition_gil_amount;
        /* Shared tail: a duplicated copy is scheduled differently and
         * only partly cross-jumped. */
        goto suspend;
    }

    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->mode = WLDCORE_SEND_UNIT_MODE_CANCEL;
    suspend:
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_wldcore_window_record_count -= 7;
        g_wldcore_window_render_record_count -= 5;
        g_wldcore_window_render_object_count -= 0xC;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }

    if (buttons & PSX_PAD_SQUARE) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_wldcore_window_records[level->list_frame.window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_frame.render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_a].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->frame_window_b].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->preview_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        level->mode = WLDCORE_SEND_UNIT_MODE_STATUS_OPEN;
        return;
    }

    if (!(buttons & PSX_PAD_CIRCLE)) {
        return;
    }

    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        if (g_wldcore_proposition_send_units[i] == value) {
            for (value = i + 1; value < 3; value++) {
                g_wldcore_proposition_send_units[value - 1] = g_wldcore_proposition_send_units[value];
            }
            g_wldcore_proposition_send_unit_count--;
            wldcore_proposition_draw_send_unit_names(level);
            wldcore_proposition_load_send_unit_candidates(level);
            world_thread_request_redraw(WLDCORE_THREAD_SLOT_WINDOW_PANEL);
            return;
        }
    }

    if (g_wldcore_proposition_send_unit_count == 3) {
        g_wldcore_window_records[level->frame_window_a].palette
            = g_wldcore_window_records[level->frame_window_b].palette = 0xA;
        g_wldcore_window_render_records[level->portrait_frame].flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_render_records[level->portrait_frame].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        g_menu_blocked_buttons = -1;
        g_wldcore_window_records[level->portrait_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
        level->shown_unit = -1;
        level->mode = WLDCORE_SEND_UNIT_MODE_REPLACE;
        level->slot = 0;
        g_wldcore_window_records[level->cursor_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->cursor_window].position.x
            = g_wldcore_window_render_records[level->list_frame.render_index].base.x + 6;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->list_frame.render_index].base.y + level->slot * 16 + 0xC;
        return;
    }

    g_wldcore_proposition_send_units[g_wldcore_proposition_send_unit_count] = value;
    g_wldcore_proposition_send_unit_count++;

    wldcore_proposition_draw_send_unit_names(level);
    wldcore_proposition_load_send_unit_candidates(level);
    world_thread_request_redraw(WLDCORE_THREAD_SLOT_WINDOW_PANEL);
}

/* Open the job report list: accepted jobs that have returned to this town, under 'Good job. You
 * came back from these jobs.'; with none, rebuild the bar rows and resume the bar. */
void wldcore_list_open_completed_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 total;
    s32 previous = g_wldcore_menu_stack_depth - 1;
    s32 loaded_count;
    s32* location;
    s32 entry_value_base;

    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }

    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_RETURNED, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        location = &g_wldcore_map_projection_state.marker.kind;
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED)
                && g_main_active_propositions[i].location == *location) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                g_wldcore_list_row_flags[count] = 0;
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }

    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x48;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count, &layout,
            &origin);
    } else {
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS;
    g_wldcore_menu_stack_depth++;
}

/* Resume handler of the completed propositions list: block Circle, Cross and Select for the
 * levels underneath and show WLDCORE_TEXT_ID_JOBS_RETURNED again. */
void wldcore_list_completed_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_RETURNED, 0);
}

/* Withhold every button from the WORLD menu frame while a level opened from the job report list
 * runs. */
void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Handle input on the job report list: Cross closes the list and Select shows
 * its help. Circle prepares the selected job's report, asks "Wanna hear about
 * ...?" and pushes the confirmation level with an OK/NO panel. */
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level) {
    s32 buttons;
    s32 depth;
    point32_t origin;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1061, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_proposition_prepare_report(
            level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index]);
        wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_text_substitution_values[0] = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_HEAR_ABOUT_JOB, 0);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE;
        g_wldcore_menu_stack_depth = depth + 1;
        g_wldcore_menu_stack_records_next[depth].proposition_report_confirm.argument
            = level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index];
    }
}

/* Input step of the "Wanna hear about ...?" OK/NO panel over the job report
 * list: Up and Down toggle the row, moving the cursor window; Cross or NO
 * releases the panel and resumes the list; OK also closes the list's panel,
 * pops both levels and pushes the job's report
 * (wldcore_menu_push_proposition_report_level).
 *
 * The `- -(...)` in the cursor-row expression is load bearing, not a typo.
 * The target's final add is `addu v0,v0,v1` (base_y first, result tied to
 * base_y's register); every plain `+` spelling of the same sum compiles to
 * `addu v1,v1,v0`. Written with a `+`, expand hands combine a plus tree with a
 * constant in it, combine reassociates the 0xE onto the scaled selection and
 * rebuilds the outer add through gen_binary, which canonicalizes the complex
 * operand into position 0 -- so the sum is tied to the delta register and the
 * following `sw` reads it. Negating the addend makes the outer operation a
 * MINUS of a NEG, which combine turns back into a plus *after* that
 * canonicalization, leaving base_y as operand 0; the rest of the block
 * (`sll` at +0x120, address materialization, `lw` at +0x130, `addiu` filling
 * the load shadow at +0x134) is unchanged. `base_y - (0 - (selection * 0x10 +
 * 0xE))` and `base_y - (0 - (0xE + selection * 0x10))` are byte-identical
 * spellings; `base_y - (-0xE - selection * 0x10)` is not (fold collapses it
 * back to a plus tree and the tie returns). */
void wldcore_proposition_handle_report_confirm_input(wldcore_proposition_report_confirm_level_t* level) {
    s32 selection;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    block_tail:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 3;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        selection = level->selection ^ 1;
        level->selection = selection;
        g_wldcore_window_records[level->cursor_window].position.y
            = g_wldcore_window_render_records[level->render_index].base.y - -(selection * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        if (level->selection != 0) {
            /* Shared tail: a duplicated release path is cross-jumped into
             * this arm instead of the cancel arm the target keeps. */
            goto block_tail;
        } else {
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count -= 1;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth -= 2;
            wldcore_menu_push_proposition_report_level(level->argument);
        }
    }
}

/* Hides the bar menu's windows and pushes WLDCORE_MENU_LEVEL_PROPOSITION_REPORT
 * for active proposition `argument`, starting the report music, or in state 99
 * (no report) for a recalled job; then closes the message box.
 */
static void wldcore_menu_push_proposition_report_level(s32 argument) {
    s32 depth = g_wldcore_menu_stack_depth;

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_records_next[depth].proposition_report.state = -1;
    g_wldcore_menu_stack_records_next[depth].proposition_report.proposition = argument;
    g_wldcore_menu_stack_records_next[depth].proposition_report.phase = 0;
    g_wldcore_menu_stack_records_next[depth].proposition_report.load_phase = WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES;
    g_wldcore_menu_stack_records_next[depth].proposition_report.render_index = -1;
    g_wldcore_menu_ordering_table_offset = 9;
    if (g_wldcore_proposition_report.recalled != 0) {
        g_wldcore_menu_stack_records_next[depth].proposition_report.state = 0x63;
    } else {
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_PROPOSITION_REPORT));
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_REPORT;
    g_wldcore_menu_stack_depth++;
}

/*
 * Steps the proposition report level: the bar's account of a finished job.
 *
 * `load_phase` first loads the proposition's report message block and then,
 * for a treasure or unexplored-land reward, its picture set. `state` then
 * walks the report one acknowledged message at a time (each message id is the
 * first of a group of six, see wldcore_proposition_step_message):
 *   0..3    the opening lines and a participant's line for the result;
 *   4..6    praise for the unit wldcore_proposition_find_preferred_job_unit
 *           names, or 7..8 when there is none;
 *   9..10   the bad-failure lines, which skip the reward;
 *   11      branches on success to 12..14 (a random participant is named as
 *           the finder, and a treasure or land picture pops in beside its
 *           caption window with the completion sound) or to 15;
 *   16..17  the closing lines for the result, after which the two picture
 *           records are dropped and the message thread is told to close;
 *   18      waits for the message thread; the next step pops this level and
 *           pushes the result level.
 * The level is pushed in state -1, or in state 99 to skip the report. Both
 * share the wait arm of 18: -1 then continues at 0 and 99 goes straight to
 * the final step.
 */
void wldcore_proposition_step_report_level(wldcore_proposition_report_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 record;
    s32 count;
    s32 participant;
    s32 load_phase;
    s32* kind;
    s32* picture;

    /* The message states below run only after the message block has finished loading. */
    load_phase = level->load_phase;
    switch (load_phase) {
    case WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES:
        if (g_wldcore_audio_queue.count != 0) {
            return;
        }
        level->load_phase = WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE;
        wldcore_load_message_block_if_changed(g_main_active_propositions[level->proposition].proposition_id
            + WLDCORE_MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST);
        return;

    case WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE:
        if (g_main_file_still_loading != 0) {
            return;
        }
        kind = &g_wldcore_proposition_report.reward_type;
        if (*kind == WLDCORE_PROPOSITION_REWARD_TREASURE) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_treasure_picture_sets[g_wldcore_proposition_report.reward_index]);
        }
        if (*kind == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
            wldcore_proposition_load_picture_data_set(
                g_wldcore_unexplored_land_picture_sets[g_wldcore_proposition_report.reward_index]);
        }
        level->load_phase = WLDCORE_PROPOSITION_REPORT_LOAD_DONE;
        break;
    }

    switch (level->state) {
    case 0:
        if (wldcore_proposition_step_message(level, 0x36) == 0) {
            return;
        }
        level->state++;
        return;

    case 1:
        if (wldcore_proposition_step_message(level, 0x3C) == 0) {
            return;
        }
        level->state++;
        return;

    case 2:
        if (wldcore_proposition_step_participant_message(level, g_wldcore_proposition_report.result * 6, -1) == 0) {
            return;
        }
        level->state++;
        return;

    case 3:
        if (wldcore_proposition_step_message(level, 0x42) == 0) {
            return;
        }
        if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_BAD_FAILURE) {
            level->state = 9;
            return;
        }

        /* The finder returns the party index + 1, or 0 when no participant qualifies. */
        count = wldcore_proposition_find_preferred_job_unit();
        g_wldcore_proposition_report.preferred_unit = count;
        if (count != 0) {
            g_wldcore_proposition_report.preferred_unit = count - 1;
            g_text_substitution_values[1] = wldcore_get_party_unit(count - 1)->job_id + TEXT_ID_JOB_NAME_BASE;
            level->state = 4;
            return;
        }
        level->state = 7;
        return;

    case 4:
        if (wldcore_proposition_step_message(level, 0x48) == 0) {
            return;
        }
        level->state++;
        return;

    case 5:
        if (wldcore_proposition_step_message(level, 0x4E) == 0) {
            return;
        }
        level->state++;
        return;

    case 6:
        if (wldcore_proposition_step_participant_message(
                level, g_wldcore_proposition_report.result * 6 + 0x12, g_wldcore_proposition_report.preferred_unit)
            == 0) {
            return;
        }
        level->state = 0xB;
        return;

    case 7:
        if (wldcore_proposition_step_message(level, 0x54) == 0) {
            return;
        }
        level->state++;
        return;

    case 8:
        if (wldcore_proposition_step_message(level, 0x5A) == 0) {
            return;
        }
        level->state = 0xB;
        return;

    case 9:
        if (wldcore_proposition_step_message(level, 0x60) == 0) {
            return;
        }
        level->state++;
        return;

    case 10:
        if (wldcore_proposition_step_message(level, 0x66) == 0) {
            return;
        }
        level->state = 0x10;
        return;

    case 11:
        if (wldcore_proposition_step_message(level, 0x6C) == 0) {
            return;
        }
        if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_SUCCESS) {
            level->state = 0xC;
            return;
        }
        level->state = 0xF;
        return;

    case 12:
        if (wldcore_proposition_step_message(level, 0x72) == 0) {
            return;
        }
        level->state++;
        return;

    case 13:
        /* Name a random participant as the finder; rand() is 15 bits, so the >> 15 gives a slot below count. */
        if (level->phase == 0) {
            participant = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices
                              [(rand()
                                   * g_main_active_propositions[g_wldcore_proposition_report.proposition_index]
                                       .participant_count)
                                  >> 15];
            if (participant == g_wldcore_proposition_report.excluded_party_index) {
                g_text_substitution_values[1]
                    = wldcore_proposition_adjust_message_index(0xE5, participant) + TEXT_ID_WORLD_MENU_BASE;
            } else {
                g_text_substitution_values[1] = participant + TEXT_ID_UNIT_NAME_BASE;
            }
        }
        if (wldcore_proposition_step_message(level, 0x78) == 0) {
            return;
        }
        level->state++;
        return;

    case 14:
        /* Wait for the picture set the load phase requested. */
        if (g_main_file_still_loading != 0) {
            return;
        }

        /* On the message's first call, add the picture and its caption window. */
        if (level->phase == 0) {
            picture = &g_wldcore_proposition_report.reward_type;
            if (*picture == WLDCORE_PROPOSITION_REWARD_TREASURE) {
                record
                    = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= WORLD_RENDER_RECORD_POP_IN;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -image.pw - 4;
                    base->y = -(image.ph >> 1) + 0x1A;
                }

                record
                    = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.x = 0;
                bounds.y = 0x70;
                bounds.w = 0x8C;
                bounds.h = 0x8C;
                window_init_text_record(record, bounds, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -0x46;
                    base->y = -0x2A;
                }
                wldcore_sound_play_effect(MAIN_SFX_COMPLETE);
            }
            if (*picture == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
                record
                    = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                level->render_index = record;
                GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
                LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
                LoadClut(image.clut, image.cx, image.cy);
                DrawSync(0);
                bounds.x = image.px;
                bounds.y = image.py;
                bounds.w = image.pw;
                bounds.h = image.ph;
                clut.x = image.cx;
                clut.y = image.cy;
                wldcore_window_init_vram_render_record(record, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
                g_wldcore_window_render_records[record].priority = 9;
                g_wldcore_window_render_records[record].flags |= WORLD_RENDER_RECORD_POP_IN;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -image.pw;
                    base->y = -(image.ph >> 1);
                }

                record
                    = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
                bounds.x = 0;
                bounds.y = 0x80;
                bounds.w = 0x88;
                bounds.h = 0x5A;
                window_init_text_record(record, bounds, 0, 0, g_wldcore_window_image_buffer);
                g_wldcore_window_render_records[record].priority = 9;
                {
                    point32_t* base = &g_wldcore_window_render_records[record].base;
                    base->x = -0x41;
                    base->y = -0x2C;
                }
                wldcore_sound_play_effect(MAIN_SFX_COMPLETE);
            }
        }
        if (wldcore_proposition_step_message(level, 0x7E) == 0) {
            return;
        }
        level->state = 0x10;
        return;

    case 15:
        if (wldcore_proposition_step_message(level, 0x84) == 0) {
            return;
        }
        level->state++;
        return;

    case 16:
        if (wldcore_proposition_step_message(level, g_wldcore_proposition_report.result * 6 + 0x8A) == 0) {
            return;
        }
        level->state++;
        return;

    case 17:
        if (wldcore_proposition_step_participant_message(level, g_wldcore_proposition_report.result * 6 + 0x24, -1)
            == 0) {
            return;
        }
        level->state++;

        /* Drop the picture and caption records that state 14 appended. */
        if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 2;
        }
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        return;

    case -1:
    case 18:
    case 99:
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        level->state++;
        return;

    default:
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_stack_depth--;
        wldcore_proposition_push_result_level(level->proposition);
        return;
    }
}

/* Build the proposition-result panel, total its gil reward, start the reporting
 * unit's message, and push the result level.
 *
 * The first-column local stays live across the text calls. Reading the result
 * through the shared record keeps its load after the level-record stores. */
static void wldcore_proposition_push_result_level(s32 proposition) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 i;
    s32 message;
    s32 count = g_main_active_propositions[proposition].participant_count;
    s32 depth;
    u16* panel_text;
    s32* party;
    point32_t* base;
    s32 rows;

    g_wldcore_proposition_gil_amount = 0;
    for (i = 0; i < count; i++) {
        g_wldcore_proposition_gil_amount += g_wldcore_proposition_report.participant_gil[i];
    }

    g_wldcore_proposition_gil_amount += g_wldcore_proposition_report.reward_value;
    g_text_substitution_values[1] = g_wldcore_proposition_gil_amount;
    /* The reward names: random bonuses from the base, unexplored lands from
     * entry 0x1b, treasures from 0x2e; entry 6 when there is no reward. */
    switch (g_wldcore_proposition_report.reward_type) {
    case WLDCORE_PROPOSITION_REWARD_TREASURE:
        g_text_substitution_values[0]
            = g_wldcore_proposition_report.reward_index + TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x2E;
        break;
    case WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND:
        g_text_substitution_values[0]
            = g_wldcore_proposition_report.reward_index + TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x1B;
        break;
    case WLDCORE_PROPOSITION_REWARD_RANDOM_BONUS:
        g_text_substitution_values[0]
            = g_wldcore_proposition_report.reward_index + TEXT_ID_PROPOSITION_REWARD_NAME_BASE;
        break;
    default:
        g_text_substitution_values[0] = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 6;
        break;
    }

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    panel_text = &g_wldcore_selected_proposition_row[0].name_text;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    rows = text_rows + 2;
    bounds.h = (count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry(index, WLDCORE_TEXT_ID_JOB_RESULT_LABELS, point, g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry(index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, g_wldcore_window_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry(
        index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, g_wldcore_window_image_buffer);
    for (i = 0; i < count; i++) {
        s32 row = i + 2;
        s32 x = 0x38;

        point.y = (text_rows + row) * 0x10 + 8;
        point.x = x;
        window_display_text_entry(index,
            g_main_active_propositions[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_wldcore_window_image_buffer);
    }

    for (i = 0; i < count; i++) {
        s32 row = i + 2;

        point.x = 0xA0;
        point.y = (text_rows + row) * 0x10 + 8;
        g_text_substitution_values[0] = g_wldcore_proposition_report.participant_jp[i];
        window_display_text_entry(index, WLDCORE_TEXT_ID_FOUR_DIGIT_VALUE, point, g_wldcore_window_image_buffer);
    }

    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x7A;
    base->y = -0x2A;
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }

    party = &g_wldcore_proposition_report.excluded_party_index;
    g_text_substitution_values[0] = *party + TEXT_ID_UNIT_NAME_BASE;
    message = 0x9C;
    script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, world_get_party_unit_formation_sprite(*party));
    if (g_wldcore_proposition_report.recalled != 0) {
        message = 0xA2;
    }

    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19,
        wldcore_proposition_adjust_message_index(message, *party) + TEXT_ID_WORLD_MESSAGE_BASE, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.proposition = proposition;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.delay = 0;
    switch (g_wldcore_proposition_report.result) {
    case WLDCORE_PROPOSITION_RESULT_SUCCESS:
        suzuki_sound_play_2_sfx(MAIN_SFX_PROPOSITION_SUCCESS_A, MAIN_SFX_PROPOSITION_SUCCESS_B);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_result.delay = 0x78;
        break;
    case WLDCORE_PROPOSITION_RESULT_FAILURE:
        suzuki_sound_play_2_sfx(MAIN_SFX_PROPOSITION_PARTIAL_A, MAIN_SFX_PROPOSITION_PARTIAL_B);
        break;
    case WLDCORE_PROPOSITION_RESULT_BAD_FAILURE:
        wldcore_sound_play_effect(MAIN_SFX_PROPOSITION_FAILURE);
        break;
    }

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].proposition_result.phase = WLDCORE_PROPOSITION_RESULT_PHASE_INPUT;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_RESULT;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Per-frame step of the proposition-result menu level.
 *
 * Phase 0 waits for confirm/cancel and then commits the result: it clears the
 * proposition variable's WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS (and sets
 * WLDCORE_PROPOSITION_VARIABLE_COMPLETED on success), adds the reward total
 * g_wldcore_proposition_gil_amount to gil (saturating at 99,999,999), records
 * the completion date (on success only) and any treasure or land date in the
 * packed nine-bit arrays, releases each participant from the
 * proposition and pays them their share, marks the proposition
 * state byte, and finally compacts the participant record out of
 * g_main_active_propositions. CLOSING waits for the message box and the audio
 * queue and restarts the bar music; DONE pops the level and hands off to the
 * countdown level or to wldcore_list_open_completed_propositions.
 *
 * The proposition, result and share 32-bit fields are read as g_wldcore_proposition_report
 * members: the target keeps that block's address in a register, which GCC
 * only does for an aggregate view, not for the scalar g_wldcore_proposition_report.proposition_index names. */
void wldcore_proposition_step_result_level(wldcore_proposition_result_level_t* level) {
    calendar_date_t date;
    s32 proposition_flags;
    s32 war_funds;
    s32 day;
    s32 month;
    s32 i;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }

    if (level->phase == WLDCORE_PROPOSITION_RESULT_PHASE_DONE) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE) {
            g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
            if (wldcore_get_completion_milestone_rank() != 0) {
                wldcore_menu_push_countdown_level();
                return;
            }
            if (g_wldcore_proposition_report.recalled == 0) {
                wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
            }
            wldcore_list_open_completed_propositions();
        }
        return;
    }

    if (level->phase == WLDCORE_PROPOSITION_RESULT_PHASE_CLOSING) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE
            && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            if (wldcore_get_completion_milestone_rank() == 0 && g_wldcore_proposition_report.recalled == 0) {
                wldcore_sound_enqueue_audio_command(
                    WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BAR));
            }
            level->phase = WLDCORE_PROPOSITION_RESULT_PHASE_DONE;
        }
        return;
    }

    if (level->delay != 0) {
        level->delay = level->delay - 1;
        if (level->delay == 0) {
            suzuki_sound_play_2_sfx(MAIN_SFX_PROPOSITION_SUCCESS_A, MAIN_SFX_PROPOSITION_SUCCESS_B);
        }
    }

    if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
        return;
    }

    g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
    g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 1;
    /* 0xFFFD clears WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS from the 16-bit value. */
    proposition_flags = script_get_variable(g_main_active_propositions[level->proposition].proposition_id
                            + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
        & 0xFFFD;
    if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        proposition_flags |= WLDCORE_PROPOSITION_VARIABLE_COMPLETED;
    }

    script_set_variable(
        g_main_active_propositions[level->proposition].proposition_id + EVENT_SCRIPT_VAR_PROPOSITION_FIRST,
        proposition_flags);
    war_funds = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + g_wldcore_proposition_gil_amount;
    if (war_funds > EVENT_WAR_FUNDS_MAX) {
        war_funds = EVENT_WAR_FUNDS_MAX;
    }

    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, war_funds);
    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        date.day = day;
        date.month = month;
        wldcore_calendar_write_date(g_main_proposition_last_attempt_date_bits,
            g_main_active_propositions[level->proposition].proposition_id, &date);
    }

    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        script_set_variable(g_wldcore_proposition_report.reward_index + EVENT_SCRIPT_VAR_TREASURE_FIRST, 1);
        date.day = day;
        date.month = month;
        wldcore_calendar_write_date(
            g_main_treasure_acquisition_date_bits, g_wldcore_proposition_report.reward_index, &date);
    }

    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        script_set_variable(g_wldcore_proposition_report.reward_index + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST, 1);
        date.day = day;
        date.month = month;
        wldcore_calendar_write_date(g_main_land_discovery_date_bits, g_wldcore_proposition_report.reward_index, &date);
    }

    for (i = 0; i < g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count; i++) {
        wldcore_get_party_unit(
            g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i])
            ->proposition_status = 0;
        main_job_add_proposition_jp(
            g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i],
            g_wldcore_proposition_report.participant_jp[i]);
    }

    /* A failed job returns to the bar after 32 days, a badly failed one after 16. */
    if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_FAILURE) {
        g_main_proposition_states[g_main_active_propositions[g_wldcore_proposition_report.proposition_index]
                .proposition_id] = WLDCORE_PROPOSITION_STATE_FAILURE | 0x20;
    }

    if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_BAD_FAILURE) {
        g_main_proposition_states[g_main_active_propositions[g_wldcore_proposition_report.proposition_index]
                .proposition_id] = WLDCORE_PROPOSITION_STATE_BAD_FAILURE | 0x10;
    }

    for (i = level->proposition; i < g_main_save_proposition_count - 1; i++) {
        g_main_active_propositions[i] = g_main_active_propositions[i + 1];
    }

    g_main_save_proposition_count--;
    if (wldcore_get_completion_milestone_rank() == 0 && g_wldcore_proposition_report.recalled == 0) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
    }

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
    level->phase = WLDCORE_PROPOSITION_RESULT_PHASE_CLOSING;
}

/* Resolve finished proposition `proposition_index` before its report: unpack its data
 * row, pick the unit who reports, decide the result and work out the JP and
 * gil, the bonus reward and the extra-days multiplier. */
static void wldcore_proposition_prepare_report(s32 proposition_index) {
    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition_index].proposition_id);
    g_wldcore_proposition_report.proposition_index = proposition_index;
    g_wldcore_proposition_report.excluded_party_index = wldcore_proposition_select_report_speaker(proposition_index);
    wldcore_proposition_determine_success();
    wldcore_proposition_calculate_base_jp_and_gil();
    wldcore_proposition_roll_bonus_reward();
    wldcore_proposition_apply_extra_days_multiplier();
}

/* Pick the participant who reports on a finished job: the only one, else the
 * one with the highest or lowest bravery or faith, as the unpacked row's
 * speaker_mode says. A mode outside 1 to 4 would return `selected` unset. The
 * goto form for the shared bravery/faith tails matches the target, which
 * reloads the stat after the compare. */
static s32 wldcore_proposition_select_report_speaker(s32 index) {
    party_unit_t* unit;
    s32 best;
    s32 i;
    s32 selected;

    if (g_main_active_propositions[index].participant_count == 1) {
        return g_main_active_propositions[index].participant_indices[0];
    }
    best = (g_wldcore_selected_proposition_row[0].speaker_mode == WLDCORE_PROPOSITION_SPEAKER_HIGHEST_BRAVERY
               || g_wldcore_selected_proposition_row[0].speaker_mode == WLDCORE_PROPOSITION_SPEAKER_HIGHEST_FAITH)
        ? -1
        : 99999;
    for (i = 0; i < g_main_active_propositions[index].participant_count; i++) {
        unit = wldcore_get_party_unit(g_main_active_propositions[index].participant_indices[i]);
        switch (g_wldcore_selected_proposition_row[0].speaker_mode) {
        case WLDCORE_PROPOSITION_SPEAKER_HIGHEST_BRAVERY:
            if (best < unit->bravery) {
                goto take_bravery;
            }
            break;
        case WLDCORE_PROPOSITION_SPEAKER_LOWEST_BRAVERY:
            if (unit->bravery < best) {
            take_bravery:
                best = unit->bravery;
                selected = g_main_active_propositions[index].participant_indices[i];
            }
            break;
        case WLDCORE_PROPOSITION_SPEAKER_HIGHEST_FAITH:
            if (best < unit->faith) {
                goto take_faith;
            }
            break;
        case WLDCORE_PROPOSITION_SPEAKER_LOWEST_FAITH:
            if (unit->faith < best) {
            take_faith:
                best = unit->faith;
                selected = g_main_active_propositions[index].participant_indices[i];
            }
            break;
        }
    }
    return selected;
}

/* Adjust message `base` for the unit who speaks it: add the entry of the 3x3
 * WLDCORE_PROPOSITION_DATA_MESSAGE_ADJUSTMENTS table for the unit's faith and
 * bravery bands (below 36, 66 and 101), and 3 more unless the unit is male.
 * A value of 101 or more would leave its band unset; stats never get there. */
s32 wldcore_proposition_adjust_message_index(s32 base, s32 party_index) {
    s32 faith_band;
    s32 bravery_band;
    u32 is_not_male;
    party_unit_t* party = wldcore_get_party_unit(party_index);
    const u8* adjustments;

    if (party->bravery < 0x65) {
        bravery_band = 2;
    }
    if (party->bravery < 0x42) {
        bravery_band = 1;
    }
    if (party->bravery < 0x24) {
        bravery_band = 0;
    }
    if (party->faith < 0x65) {
        faith_band = 2;
    }
    if (party->faith < 0x42) {
        faith_band = 1;
    }
    if (party->faith < 0x24) {
        faith_band = 0;
    }
    is_not_male = party->gender_flags & UNIT_FLAG_MALE;
    is_not_male = is_not_male < 1;
    adjustments = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_MESSAGE_ADJUSTMENTS);
    return base + adjustments[faith_band * 3 + bravery_band] + is_not_male * 3;
}

/*
 * Queue a proposition message and wait for its acknowledgment.
 *
 * The first call shows message `message`, adjusted for the reporting unit and
 * with its portrait; return 1 and clear the caller's phase once Circle is
 * pressed with the message box ready. Until then return 0.
 */
static s32 wldcore_proposition_step_message(wldcore_proposition_report_level_t* state, s32 message) {
    if (state->phase == 0) {
        s32* party_index;
        state->phase = 1;
        if (!thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE))
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        party_index = &g_wldcore_proposition_report.excluded_party_index;
        g_text_substitution_values[0] = *party_index + TEXT_ID_UNIT_NAME_BASE;
        script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, world_get_party_unit_formation_sprite(*party_index));
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19,
            wldcore_proposition_adjust_message_index(message, *party_index) + TEXT_ID_WORLD_MESSAGE_BASE, 0);
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        if (world_menu_get_event_state_flag()) {
            state->phase = 0;
            return 1;
        }
    }
    return 0;
}

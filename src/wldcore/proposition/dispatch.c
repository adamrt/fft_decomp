/* The bar dispatches propositions by choosing participants and confirming their assignment; day counters track their
 * return. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message_box.h"
#include "fft/party.h"
#include "fft/portrait.h"
#include "fft/proposition.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s32 g_proposition_send_unit_count;
extern s16 g_proposition_candidate_levels[];

/* Scalar view of g_window_panel_render_state.select_text_table: as a
 * struct field its address would share the selected_index base register, but
 * the target stores it by symbol. */
extern s16 g_window_panel_scroll_offset;

static s32 candidates_build(proposition_send_unit_level_t* level);

static s32 g_proposition_send_units[3] = { 0 };

/* Pushes MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM, the summary of a job
 * about to be dispatched: a cursor window, a header window, the OK/NO record
 * and a summary record with "Job contents:" and the job's contents text
 * (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1 + name_text), the days and place
 * labels (TEXT_ID_JOB_CONDITION_LABELS) and one row per chosen unit.
 * The message box asks TEXT_ID_CONFIRM_CONDITIONS; both windows are
 * placed against the OK/NO record.
 *
 * `unused` reproduces the target's 0x78-byte frame. The row offset uses its
 * own local: reusing `row` (then set twice) schedules the loop's `addiu v1`
 * ahead of `move a0`. */
void proposition_dispatch_confirm_open(void) {
    RECT bounds;
    point32_t point;
    text_dimensions_t size;
    s32 unused[8];
    point32_t* position;
    point32_t* base;
    u16* location;
    s32 index;
    s32 depth;
    s32 i;
    s32 row;
    s32 line;

    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CONFIRM_CONDITIONS, 0);

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.header_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CHOICE_PANEL_HEADER;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_text_record_init(index, bounds, 0, TEXT_ID_OK_NO, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 8;
    position = &g_window_gs_image_records[index].base;
    position->x = 0x48;
    position->y = 0x20;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    /* The summary panel's render record index lands in the 32-bit row field, which is cleared below. */
    g_menu_stack_records[g_menu_stack_depth].dispatch_confirm.row = index;
    location = &g_proposition_selected_row.name_text;
    text_pixels_measure(
        &size.width, &size.rows, text_entry_find(*location + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.x = 0;
    row = size.rows + 2;
    bounds.h = (g_proposition_send_unit_count - -row) * 0x10 + 0x10;
    g_text_substitution_values[1] = g_proposition_dispatch_days;
    g_text_substitution_values[0] = g_proposition_dispatch_days;
    g_text_substitution_values[2] = g_world_map_marker_state.marker.location_id + TEXT_ID_LOCATION_NAME_BASE;
    window_build_render_record_image(index, bounds, 0, &g_window_image_buffer[0x200]);
    point.x = 8;
    point.y = size.rows * 0x10 + 8;
    window_display_text_entry(index, TEXT_ID_JOB_CONDITION_LABELS, point, &g_window_image_buffer[0x200]);
    point.x = 8;
    point.y = 8;
    window_display_text_entry(index, TEXT_ID_JOB_CONTENTS_LABEL, point, &g_window_image_buffer[0x200]);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry(
        index, *location + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, &g_window_image_buffer[0x200]);
    for (i = 0; i < g_proposition_send_unit_count; i++) {
        point.x = 0x3C;
        line = i + 2;
        point.y = (size.rows + line) * 0x10 + 8;
        window_display_text_entry(
            index, g_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, &g_window_image_buffer[0x200]);
    }
    window_load_render_record_image(index, &g_window_image_buffer[0x200]);
    g_window_gs_image_records[index].priority = 8;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_gs_image_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;

    depth = g_menu_stack_depth;
    index = g_menu_stack_records[depth].dispatch_confirm.render_index;
    g_menu_stack_records[depth].dispatch_confirm.row = 0;
    g_window_records[g_menu_stack_records[depth].dispatch_confirm.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].dispatch_confirm.cursor_window].position.y
        = g_window_gs_image_records[index].base.y - -(g_menu_stack_records[depth].dispatch_confirm.row * 0x10 + 0xE);
    g_window_records[g_menu_stack_records[depth].dispatch_confirm.header_window].position.x
        = g_window_gs_image_records[index].base.x + 3;
    g_menu_stack_depth = depth + 1;
    g_window_records[g_menu_stack_records[depth].dispatch_confirm.header_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM;
}

/* Input step of the dispatch confirmation's OK/NO panel: up/down toggle the
 * row, Cross and NO close the panel; OK pays g_proposition_gil_amount
 * from the war funds, marks the proposition attempted and in progress,
 * records it in the next g_proposition_active slot with its
 * participants (each marked busy), and closes the panel. Either way the
 * propositions list reopens. */
void proposition_dispatch_confirm_update(menu_stack_record_t* level) {
    s32 row;
    s32 i;

    if (g_window_gs_image_records[level->dispatch_confirm.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and both confirm paths
         * jump into it; duplicated copies cross-jump into the last one. */
    block_tail:
        g_hud_value_mode = HUD_VALUE_FUNDS;
        g_window_record_count -= 2;
        g_window_render_record_count -= 2;
        g_window_render_object_count -= 4;
        g_menu_stack_depth -= 1;
        proposition_available_list_open();
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0 || world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        row = level->dispatch_confirm.row ^ 1;
        level->dispatch_confirm.row = row;
        g_window_records[level->dispatch_confirm.cursor_window].position.y
            = g_window_gs_image_records[level->dispatch_confirm.render_index].base.y - -(row * 0x10 + 0xE);
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        if (level->dispatch_confirm.row != 1) {
            script_variable_set(
                EVENT_SCRIPT_VAR_WAR_FUNDS, script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) - g_proposition_gil_amount);
            g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
            script_variable_set(g_proposition_selected_row.id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_variable_get(g_proposition_selected_row.id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | PROPOSITION_VARIABLE_ATTEMPTED);
            g_proposition_active[g_proposition_count].flags = PROPOSITION_PROGRESS_DISPATCHED;
            g_proposition_active[g_proposition_count].proposition_id = g_proposition_selected_row.id - 1;
            g_proposition_active[g_proposition_count].elapsed_days = 0;
            g_proposition_active[g_proposition_count].assigned_days = g_proposition_dispatch_days;
            g_proposition_active[g_proposition_count].location = g_world_map_marker_state.marker.location_id;
            g_proposition_active[g_proposition_count].participant_count = g_proposition_send_unit_count;
            for (i = 0; i < g_proposition_send_unit_count; i++) {
                party_roster_unit_get(g_proposition_send_units[i])->proposition_status = 1;
                g_proposition_active[g_proposition_count].participant_indices[i] = g_proposition_send_units[i];
            }
            g_proposition_count++;
            script_variable_set(g_proposition_selected_row.id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1),
                script_variable_get(g_proposition_selected_row.id + (EVENT_SCRIPT_VAR_PROPOSITION_FIRST - 1))
                    | PROPOSITION_VARIABLE_IN_PROGRESS);
        }
        goto block_tail;
    }
}

/*
 * Advance daily proposition progress and packed countdown bytes.
 *
 * Every active proposition not yet returned counts one more elapsed day, and
 * returns (PROPOSITION_PROGRESS_RETURNED) when that reaches its
 * assigned days or it was recalled. Each proposition's days until it is
 * offered again drop by one, from at most 32, keeping its failure bits.
 */
void proposition_day_advance(void) {
    s32 i;
    u8* counter;

    for (i = 0; i < g_proposition_count; i++) {
        if (!(g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED)) {
            g_proposition_active[i].elapsed_days++;
            if (g_proposition_active[i].assigned_days == g_proposition_active[i].elapsed_days
                || (g_proposition_active[i].flags & PROPOSITION_PROGRESS_RECALLED)) {
                g_proposition_active[i].flags |= PROPOSITION_PROGRESS_RETURNED;
            }
        }
    }
    counter = g_proposition_states;
    do {
        u32 value = *counter;
        s32 days = value & PROPOSITION_STATE_DAYS_MASK;
        if (days != 0) {
            if (days > 32) {
                days = 32;
            }
            days--;
            *counter = (value & (PROPOSITION_STATE_FAILURE | PROPOSITION_STATE_BAD_FAILURE)) | days;
        }
        counter++;
    } while ((s32)counter < (s32)(g_proposition_states + EVENT_SCRIPT_VAR_PROPOSITION_COUNT)); /* signed slt */
}

/*
 * Count the active propositions that have returned
 * (PROPOSITION_PROGRESS_RETURNED) to a location, or to any location
 * when `location` is -1.
 */
s32 proposition_count_returned(s32 location) {
    s32 i;
    s32 count = 0;
    for (i = 0; i < g_proposition_count; i++) {
        if (g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED) {
            if (location == -1 || g_proposition_active[i].location == location)
                count++;
        }
    }
    return count;
}

/* Push the send-unit level (MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT).
 *
 * Builds the available-unit list into the new level record, starts the panel
 * thread on it with each unit's level as a second column, appends the level's
 * seven window records and five render records, and asks
 * TEXT_ID_CHOOSE_UNITS.
 *
 * The selected index is written through the panel render state:
 * the target derives the thread parameter from that field's address. Each
 * record's x/y pair is written through a block-local pointer so the pointer
 * shares the index product's register, and the chained frame_index/anim_counter
 * store keeps anim_counter's base in a register as in the target. */
void proposition_participants_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    RECT bounds;
    s32 index;
    s32 depth;
    s32 count;

    g_window_panel_scroll_offset = 0;
    g_window_panel_render_state.selected_index = 0;
    g_proposition_send_unit_count = 0;
    count = candidates_build(&g_menu_stack_records[g_menu_stack_depth].send_unit);

    layout.text_width = 0x6E;
    layout.visible_rows = count;
    origin.x = 8;
    origin.y = 0x50;
    window_panel_state_init(0xC, g_menu_stack_records[g_menu_stack_depth].send_unit.unit_count, &layout, &origin);

    /* The initializer hides label column 1; show it with each candidate's level as digits. */
    g_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_window_panel_label_state.x[1] = 0x58;
    g_window_panel_label_state.text_ids[1] = g_proposition_candidate_levels;
    g_window_panel_label_state.text_colors[1] = g_menu_stack_row_text_colors;
    g_window_panel_label_state.x[0] = 0;
    g_window_panel_render_state.header_id = 0x10;
    g_window_panel_render_state.selected_index = 0;
    g_window_panel_scroll_offset = 0;
    thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, screen_text_list_thread);
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_frame.window_index = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_window_records[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_window_records[index].sequence = WINDOW_ANIM_PROPOSITION_COUNT_LABEL;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 9;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_frame.render_index = index;
    bounds.x = 0x80;
    bounds.y = 0;
    bounds.w = 0x6C;
    bounds.h = 0x40;
    window_text_record_init(index, bounds, 0, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 9;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.frame_window_a = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_window_records[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_window_records[index].sequence = WINDOW_ANIM_SEND_UNIT_FRAME_A;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.frame_window_b = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_window_records[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_window_records[index].sequence = WINDOW_ANIM_SEND_UNIT_FRAME_B;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.preview_render = index;
    g_window_gs_image_records[index].priority = 9;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.list_render = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_text_record_init(index, bounds, 0, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 9;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_render = index;
    g_window_gs_image_records[index].priority = 9;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    window_text_record_init(index, bounds, 0, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 9;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.portrait_window = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_window_records[index].priority = 9;
        g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = 0x3E;
        position->y = 0x28;
    }
    g_window_records[index].sequence = WINDOW_ANIM_SEND_UNIT_PORTRAIT;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.left_arrow = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_window_records[index].priority = 9;
        g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = -0x74;
        position->y = -0x69;
    }
    g_window_records[index].sequence = WINDOW_ANIM_L1_BUTTON;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].send_unit.right_arrow = index;
    {
        point32_t* position = &g_window_records[index].position;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        g_window_records[index].priority = 9;
        g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
        position->x = 0x60;
        position->y = -0x69;
    }
    g_window_records[index].sequence = WINDOW_ANIM_R1_BUTTON;
    g_window_records[index].frame_index = g_window_records[index].anim_counter = 0;

    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_UNITS, 0);

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].send_unit.shown_preview = -1;
    g_menu_stack_records[depth].send_unit.mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT;
    g_menu_stack_depth = depth + 1;
}

/* Build the list of available non-guest human units for a proposition.
 *
 * Units already assigned to a proposition, absent roster entries, monsters,
 * and special-character sprite sets are excluded. */
static s32 candidates_build(proposition_send_unit_level_t* level) {
    s32 i = 0;
    s32 j;
    party_unit_t* unit;

    level->unit_count = 0;
    for (; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = party_roster_unit_get(i);
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
        for (j = 0; j < g_proposition_send_unit_count; j++) {
            if (g_proposition_send_units[j] == i) {
                break;
            }
        }
        if (j < g_proposition_send_unit_count) {
            g_menu_stack_row_text_colors[level->unit_count] = 8;
        } else {
            g_menu_stack_row_text_colors[level->unit_count] = 0;
        }
        g_menu_stack_entry_values[level->unit_count] = i + TEXT_ID_UNIT_NAME_BASE;
        g_proposition_candidate_levels[level->unit_count] = unit->level;
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
static void candidate_names_draw(proposition_send_unit_level_t* level) {
    point32_t point;
    s32 index = level->list_frame.render_index;
    s32 i;

    screen_window_frame_image_build(g_window_gs_image_records[index].bounds.w,
        g_window_gs_image_records[index].bounds.h, (u16*)g_window_image_buffer);
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_proposition_send_unit_count; i++) {
        window_display_text_entry(
            index, g_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);
}

/* Point render record `index` at the formation portrait of party member
 * `party_index`: its texture page, cell rectangle and CLUT (16 colors at
 * x 0x280 + 0x40 per 40 portraits + 16 * (code % 4), y 0x1F0 +
 * (code % 40) / 4), at full brightness, and show the record. The first
 * argument is not read. */
static void candidate_portrait_show(proposition_send_unit_level_t* unused, s32 party_index, s32 index) {
    POLY_FT4 poly;
    s32 code;
    s32 cell;
    CVECTOR* color;
    RECT* bounds;

    code = portrait_party_unit_code_get(party_index);
    cell = code;
    portrait_build_wldface_poly(&poly, code);
    g_window_gs_image_records[index].clut.x = (code / 40 << 6) + 0x280 + cell % 4 * 16;
    g_window_gs_image_records[index].clut.y = cell % 40 / 4 + 0x1F0;
    g_window_gs_image_records[index].tpage = poly.tpage;
    bounds = &g_window_gs_image_records[index].bounds;
    bounds->x = poly.u0;
    bounds->y = poly.v0;
    bounds->w = poly.u1 - poly.u0;
    bounds->h = poly.v2 - poly.v0;
    color = &g_window_gs_image_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_window_gs_image_records[index].anim_counter = 0;
    g_window_gs_image_records[index].flags &= ~DISPLAY_OBJECT_HIDDEN;
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
static void participant_slots_update(proposition_send_unit_level_t* level) {
    point32_t unused;
    u32 buttons;
    s32 unit;

    if ((g_window_gs_image_records[level->portrait_frame].flags & DISPLAY_OBJECT_POP_IN) == 0) {
        unit = g_proposition_send_units[level->slot] & 0x7FF;
        if (unit != level->shown_unit) {
            level->shown_unit = unit;
            candidate_portrait_show(level, unit, level->portrait_render);
        }
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Shared tail: a duplicated close path cross-jumps to the same
         * layout, but without this label the scheduler reorders its loads. */
        goto close;
    }

    if ((buttons & PSX_PAD_CIRCLE) != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        unit = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        g_proposition_send_units[level->slot] = unit;
        candidate_names_draw(level);
        candidates_build(level);
        screen_thread_redraw_request(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
    close:
        g_window_records[level->frame_window_a].palette = g_window_records[level->frame_window_b].palette = 0;
        g_window_gs_image_records[level->portrait_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->portrait_frame].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->portrait_window].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        level->mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (level->slot == 0) {
            level->slot = 2;
        } else {
            level->slot--;
        }
    } else if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        if (level->slot == 2) {
            level->slot = 0;
        } else {
            level->slot++;
        }
    } else {
        return;
    }

    sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    g_window_records[level->cursor_window].position.x
        = g_window_gs_image_records[level->list_frame.render_index].base.x + 6;
    g_window_records[level->cursor_window].position.y
        = g_window_gs_image_records[level->list_frame.render_index].base.y + level->slot * 16 + 12;
}

/* Build the status panel data of party member `party_index` and start the
 * three unit panel threads on it (status banner, numeric panel, equipment
 * and ability panel), raising SYSTEM_FLAG_UNIT_PANEL_THREADS. */
static void participant_panels_start(s32 party_index) {
    unit_panel_thread_t* thread8;

    unit_panel_party_unit_prepare(party_index);
    thread8 = &g_unit_panel_banner_params;
    g_unit_panel_numbers_params.position.y = 0;
    g_unit_panel_panel_params.position.y = 0;
    g_unit_panel_banner_params.position.y = 0;
    thread8->position.x = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)thread8, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_panel_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_numbers_params, 0, 0);
    g_system_flags |= SYSTEM_FLAG_UNIT_PANEL_THREADS;
}

/* Status view of the send-unit level: once the panel threads stop it opens the unit's status
 * screens; L1 and R1 page through the candidates with wraparound, Select opens the layout window,
 * Circle or Cross close the view and return to 'So, who you wanna send?'. */
static void participant_status_update(proposition_send_unit_level_t* level) {
    u32 buttons;
    s32 selected;
    s32 direction;
    s16* menu_selected_index;

    if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) != 0) {
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_window_records[level->left_arrow].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->right_arrow].flags ^= DISPLAY_OBJECT_HIDDEN;
        level->mode = PROPOSITION_SEND_UNIT_MODE_STATUS;
        menu_selected_index = &g_window_panel_render_state.selected_index;
        participant_panels_start(g_menu_stack_entry_values[*menu_selected_index] & TEXT_ID_ENTRY_MASK);
        level->slot = *menu_selected_index;
    } else if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS) {
        buttons = g_world_map_input_new_button_presses;
        if (buttons & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            g_window_records[level->left_arrow].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[level->right_arrow].flags |= DISPLAY_OBJECT_HIDDEN;
            level->mode = PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE;
            thread_parameters_set(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_parameters_set(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
            thread_parameters_set(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }
        if (buttons & PSX_PAD_SELECT) {
            g_window_records[level->left_arrow].palette = g_window_records[level->right_arrow].palette = 0xA;
            sound_play_effect(SOUND_SFX_CONFIRM);
            unit_panel_help_open();
            return;
        }
        if (!(g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_window_records[level->left_arrow].sequence = WINDOW_ANIM_L1_BUTTON;
            g_window_records[level->right_arrow].sequence = WINDOW_ANIM_R1_BUTTON;
            g_window_records[level->left_arrow].frame_index = g_window_records[level->left_arrow].anim_counter
                = g_window_records[level->right_arrow].frame_index = g_window_records[level->right_arrow].anim_counter
                = 0;
        }
        if (world_map_input_repeat_update(PSX_PAD_R1) != 0
            && g_window_records[level->left_arrow].sequence != WINDOW_ANIM_L1_BUTTON_PRESS) {
            g_window_records[level->right_arrow].sequence = WINDOW_ANIM_R1_BUTTON_PRESS;
            g_window_records[level->right_arrow].frame_index = g_window_records[level->right_arrow].anim_counter = 0;
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
            if (world_map_input_repeat_update(PSX_PAD_L1) == 0
                || g_window_records[level->right_arrow].sequence == WINDOW_ANIM_R1_BUTTON_PRESS) {
                return;
            }
            g_window_records[level->left_arrow].sequence = WINDOW_ANIM_L1_BUTTON_PRESS;
            g_window_records[level->left_arrow].frame_index = g_window_records[level->left_arrow].anim_counter = 0;
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
        g_unit_panel_banner_params.redraw_request = direction;
        unit_panel_party_unit_prepare(g_menu_stack_entry_values[level->slot] & TEXT_ID_ENTRY_MASK);
        g_unit_panel_panel_params.redraw_request = 1;
        g_unit_panel_numbers_params.redraw_request = 1;
    done:
        sound_play_effect(SOUND_SFX_PAGE_SWITCH);
    } else if (level->mode == PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE) {
        if (thread_is_running(THREAD_SCREEN_SLOT_UNIT_STATUS) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_NUMERIC_PANEL) != 0) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        level->mode = PROPOSITION_SEND_UNIT_MODE_SELECT;
        g_system_flags &= ~(SYSTEM_FLAG_HIDE_CONTEXT_VALUE | SYSTEM_FLAG_UNIT_PANEL_THREADS);
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CHOOSE_UNITS, 0);
        thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, screen_text_list_thread);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);
        g_window_records[level->list_frame.window_index].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->list_frame.render_index].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->frame_window_a].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->frame_window_b].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->preview_render].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->list_render].flags ^= DISPLAY_OBJECT_HIDDEN;
    }
}

/* Undo proposition_participants_suspend when a sub-level closes:
 * default palettes on the send-unit windows and a neutral preview portrait. */
void proposition_participants_resume(menu_stack_record_t* level) {
    CVECTOR* color;

    g_window_records[level->send_unit.list_frame.window_index].palette
        = g_window_gs_image_records[level->send_unit.list_frame.render_index].palette
        = g_window_gs_image_records[level->send_unit.list_render].palette
        = g_window_records[level->send_unit.cursor_window].palette = 0;
    /* Each channel store reloads the level's render index. */
    color = &g_window_gs_image_records[level->send_unit.preview_render].color;
    color->r = 0x80;
    color = &g_window_gs_image_records[level->send_unit.preview_render].color;
    color->g = 0x80;
    color = &g_window_gs_image_records[level->send_unit.preview_render].color;
    color->b = 0x80;
    g_window_records[level->send_unit.left_arrow].palette = g_window_records[level->send_unit.right_arrow].palette = 0;
}

/* Gray out the send-unit windows (palettes 6, 10 and 2) and tint the preview
 * portrait blue (0x40, 0x50, 0x70) while a sub-level runs over them; undone by
 * proposition_participants_resume. */
void proposition_participants_suspend(menu_stack_record_t* level) {
    s32 pad[4];

    g_window_records[level->send_unit.list_frame.window_index].palette = 6;
    g_window_gs_image_records[level->send_unit.list_frame.render_index].palette = 2;
    g_window_gs_image_records[level->send_unit.list_render].palette = 2;
    g_window_records[level->send_unit.cursor_window].palette = 10;
    g_window_gs_image_records[level->send_unit.preview_render].color.r = 0x40;
    g_window_gs_image_records[level->send_unit.preview_render].color.g = 0x50;
    g_window_gs_image_records[level->send_unit.preview_render].color.b = 0x70;
}

/* Top-level step of the proposition send-unit menu level, dispatching on the
 * level's mode.
 *
 * CANCEL and ACCEPT wait for the panel thread to close and then pop the level,
 * ACCEPT continuing into the proposition detail level and its days panel; the
 * STATUS modes run participant_status_update and REPLACE runs
 * participant_slots_update.
 *
 * Otherwise (SELECT) the highlighted unit's portrait is shown in the preview
 * record once the list has popped in, then the new button presses are
 * handled: Select opens help message 0x109a, Start (with at least one chosen
 * unit) accepts, Cross cancels, Square hides the list and opens the unit's
 * status panels, and Circle toggles the highlighted unit in or out of the
 * three g_proposition_send_units slots (removing it compacts the
 * list; adding one when all three are full switches to REPLACE).
 *
 * The target reserves a 0x30-byte frame but only saves s0 and ra; the unused
 * 16-byte array reproduces that frame. The removal and addition paths each
 * end in their own redraw calls, which jump2 cross-jumps from the count
 * store onward. */
void proposition_participants_update(menu_stack_record_t* level) {
    s32 buttons;
    s32 value;
    s32 i;
    s32 unused[4];

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_CANCEL) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_system_flags &= ~SYSTEM_FLAG_INPUT_BLOCKED;
            g_menu_stack_depth--;
            proposition_available_list_open();
        }
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_ACCEPT) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
            g_menu_blocked_buttons = 0;
            g_system_flags &= ~SYSTEM_FLAG_INPUT_BLOCKED;
            g_menu_stack_depth--;
            proposition_detail_level_open(g_proposition_selected_row.id - 1);
            proposition_detail_level_suspend(level);
            proposition_days_open();
        }
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN
        || level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS
        || level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE) {
        participant_status_update(&level->send_unit);
        return;
    }

    if (level->send_unit.mode == PROPOSITION_SEND_UNIT_MODE_REPLACE) {
        participant_slots_update(&level->send_unit);
        return;
    }

    if (!(g_window_gs_image_records[level->send_unit.list_frame.render_index].flags & DISPLAY_OBJECT_POP_IN)) {
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
        if (value != level->send_unit.shown_preview) {
            level->send_unit.shown_preview = value;
            candidate_portrait_show(&level->send_unit, value, level->send_unit.preview_render);
        }
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_SELECT) {
        proposition_participants_suspend(level);
        menu_stack_message_open(0x109A, 1);
        return;
    }

    if (buttons & PSX_PAD_START) {
        if (g_proposition_send_unit_count == 0) {
            sound_play_effect(SOUND_SFX_INVALID);
            return;
        }
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_ACCEPT;
        g_hud_value_mode = HUD_VALUE_COST_AND_FUNDS;
        g_hud_value_secondary = g_proposition_gil_amount;
        /* Shared tail: a duplicated copy is scheduled differently and
         * only partly cross-jumped. */
        goto suspend;
    }

    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_CANCEL;
    suspend:
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_window_record_count -= 7;
        g_window_render_record_count -= 5;
        g_window_render_object_count -= 0xC;
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        return;
    }

    if (buttons & PSX_PAD_SQUARE) {
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_window_records[level->send_unit.list_frame.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->send_unit.list_frame.render_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->send_unit.frame_window_a].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->send_unit.frame_window_b].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->send_unit.preview_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->send_unit.list_render].flags |= DISPLAY_OBJECT_HIDDEN;
        g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN;
        return;
    }

    if (!(buttons & PSX_PAD_CIRCLE)) {
        return;
    }

    sound_play_effect(SOUND_SFX_CONFIRM);
    value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK;
    for (i = 0; i < g_proposition_send_unit_count; i++) {
        if (g_proposition_send_units[i] == value) {
            for (value = i + 1; value < 3; value++) {
                g_proposition_send_units[value - 1] = g_proposition_send_units[value];
            }
            g_proposition_send_unit_count--;
            candidate_names_draw(&level->send_unit);
            candidates_build(&level->send_unit);
            screen_thread_redraw_request(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
            return;
        }
    }

    if (g_proposition_send_unit_count == 3) {
        g_window_records[level->send_unit.frame_window_a].palette
            = g_window_records[level->send_unit.frame_window_b].palette = 0xA;
        g_window_gs_image_records[level->send_unit.portrait_frame].flags |= DISPLAY_OBJECT_POP_IN;
        g_window_gs_image_records[level->send_unit.portrait_frame].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_menu_blocked_buttons = -1;
        g_window_records[level->send_unit.portrait_window].flags ^= DISPLAY_OBJECT_HIDDEN;
        level->send_unit.shown_unit = -1;
        level->send_unit.mode = PROPOSITION_SEND_UNIT_MODE_REPLACE;
        level->send_unit.slot = 0;
        g_window_records[level->send_unit.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->send_unit.cursor_window].position.x
            = g_window_gs_image_records[level->send_unit.list_frame.render_index].base.x + 6;
        g_window_records[level->send_unit.cursor_window].position.y
            = g_window_gs_image_records[level->send_unit.list_frame.render_index].base.y + level->send_unit.slot * 16
            + 0xC;
        return;
    }

    g_proposition_send_units[g_proposition_send_unit_count] = value;
    g_proposition_send_unit_count++;

    candidate_names_draw(&level->send_unit);
    candidates_build(&level->send_unit);
    screen_thread_redraw_request(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL);
}

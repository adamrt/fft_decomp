#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Pushes the proposition confirmation level (type 0x13) for active
 * proposition `proposition`: unpacks its proposition row, hides the parent
 * level's two windows and render record, shows message 0xb81d, and builds a
 * two-row cursor/frame window pair over render record text 0xb805 beside the
 * proposition detail panel.
 *
 * Every level access re-indexes g_wldcore_menu_stack_depth rather than a
 * local copy, which is what keeps the depth in the target's registers. The
 * target reserves 0x30 bytes of frame the function never uses. */
void wldcore_proposition_push_cancel_confirm_level(s32 proposition) {
    RECT bounds;
    u8 unused[0x30];
    s32 index;
    s32 depth;
    point32_t* base;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.first_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.second_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_pair_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CALL_BACK_PROMPT, 0);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.frame_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.choice_render = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_render_records[index].priority = 8;
    base->x = 0x48;
    base->y = 0x20;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.cursor_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.cursor_window]
        .position.y = g_wldcore_window_render_records[index].base.y + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.frame_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.frame_window]
        .position.y = g_wldcore_window_render_records[index].base.y - 2;
    wldcore_list_build_proposition_detail_panel_image(&g_wldcore_menu_stack_records_next[depth], proposition, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.choice = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.confirmed = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_CANCEL_CONFIRM;
    g_wldcore_menu_stack_depth++;
}

/* Build the image of the proposition detail panel for g_main_active_propositions[proposition]:
 * the elapsed/assigned day counts and the location go into text substitution
 * 32-bit values 0-2, and one 16-pixel row is drawn per participant.
 *
 * Both menu variants share the detail render index and row count.
 *
 * The panel's row count is the measured row count of text g_wldcore_selected_proposition_row[0].name_text +
 * 0xf7ff plus two, kept in the level record at +0x0c.
 *
 * The target stores base_x/base_y through one pointer to the pair, and reads
 * every progress field through the array form g_main_active_propositions[proposition], which
 * is what produces its four-instruction ASPSX $at indexed loads.
 *
 * `panel_text` keeps the text-id source's address in a saved register across
 * both reads, as the target does. Assigning bounds.x before
 * position.y is what puts the two by-value halves in the target's registers. */
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_stack_record_t* level, s32 proposition, s32 coordinate_mode) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 rows;
    s32 i;
    u16* panel_text;
    point32_t* base;

    panel_text = &g_wldcore_selected_proposition_row[0].name_text;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    level->proposition_detail_panel.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    rows = text_rows + 2;
    level->proposition_detail_panel.panel_rows = rows;
    g_text_substitution_values[0] = g_main_active_propositions[proposition].elapsed_days;
    g_text_substitution_values[1] = g_main_active_propositions[proposition].assigned_days;
    g_text_substitution_values[2] = g_main_active_propositions[proposition].location | TEXT_ID_LOCATION_NAME_BASE;
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = (g_main_active_propositions[proposition].participant_count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, coordinate_mode, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONDITION_LABELS, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry_variant(index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point,
        g_wldcore_proposition_detail_image_buffer);
    for (i = 0; i < g_main_active_propositions[proposition].participant_count; i++) {
        s32 row = i + 2;

        point.x = 0x3C;
        point.y = (text_rows + row) * 0x10 + 8;
        window_display_text_entry_variant(index,
            g_main_active_propositions[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_wldcore_proposition_detail_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_proposition_detail_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;
}

/* Input step for the two-row proposition confirmation window: up/down toggle
 * the highlighted row and move the cursor window, Circle either
 * accepts the proposition and starts WORLD thread 14 with text 0xb81e or
 * closes the level, and Cross closes it. */
void wldcore_proposition_handle_cancel_confirm_input(wldcore_menu_stack_record_t* level) {
    s32 choice;

    if (g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].flags
        & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->proposition_cancel_confirm.confirmed != 0) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            goto close;
        }
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and the other close paths
         * jump into it; duplicated copies cross-jump into the last one. */
    close:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_propositions();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        choice = level->proposition_cancel_confirm.choice ^ 1;
        level->proposition_cancel_confirm.choice = choice;
        g_wldcore_window_records[level->proposition_cancel_confirm.cursor_window].position.y
            = g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].base.y
            - -(choice * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (!(g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->proposition_cancel_confirm.confirmed != 0) {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    if (level->proposition_cancel_confirm.choice == 1) {
        goto close;
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CALL_BACK_ACCEPTED, 0);
    level->proposition_cancel_confirm.confirmed = 1;
    g_main_active_propositions[level->proposition_cancel_confirm.proposition].flags
        |= WLDCORE_PROPOSITION_PROGRESS_RECALLED;
    g_wldcore_window_records[level->proposition_cancel_confirm.cursor_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[level->proposition_cancel_confirm.frame_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

/*
 * Pushes the sound-novel menu level (WLDCORE_MENU_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_wldcore_saved_record_index.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (wldcore_sound_novel_restore_saved_state), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
void wldcore_menu_push_sound_novel_level(s32 argument) {
    RECT bounds;
    point32_t origin;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    world_animated_render_record_t* window4;
    s32 bar_render;
    s32 index;
    point32_t* pos1;
    point32_t* pos2;
    point32_t* pos3;
    point32_t* pos4;
    point32_t* render_pos;

    /* Held from entry: with every saved register taken, reload rematerialises
     * the constant beside its one use, which is the target's la placement. */
    render_pos = &g_wldcore_window_render_records[0].base;
    g_wldcore_saved_context_value_display_position = g_wldcore_context_value_display_position;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    wldcore_gfx_clear_vram_and_scratch(1);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.message = argument;

    bar_render
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.left_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x7A;
    pos1->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.right_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x42;
    pos2->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.render_index = index;
    bounds.x = 0x1E4;
    bounds.y = 0x1E8;
    bounds.w = 0x20;
    bounds.h = 8;
    origin.x = 0x20;
    origin.y = 0x1E8;
    wldcore_window_init_vram_render_record(index, bounds, origin, 0);
    pos3 = (point32_t*)((s32)render_pos + index * sizeof(wldcore_window_render_record_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.overlay_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_OVERLAY;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;
    window4 = &g_wldcore_window_records[index];
    window4->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos4 = &g_wldcore_window_records[index].position;
    pos4->x = 0x54;
    pos4->y = 0x58;
    g_wldcore_window_records[index].palette = 0;

    suzuki_sound_append_resource(g_wldcore_sound_novel_sound_resource);
    wldcore_sound_novel_restore_saved_state(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_screen_fade_state.priorities[0] = 8;
    g_wldcore_screen_fade_state.flags[1] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel._unknown_20 = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.countdown = 0x270F;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.fade_timer = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.phase = WLDCORE_SOUND_NOVEL_PHASE_RUN;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL;
    g_wldcore_menu_stack_depth++;
    wldcore_fade_start_screen(0, 0x10);
}

/* Proposition yes/no confirmation panel and the days number-panel level with its thread wait. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Pushes the two-row confirmation panel (text 0xb805) whose render record
 * sits at origin; wldcore_proposition_handle_dispatch_confirm_input services the level.
 *
 * The origin arrives by value: the target spills a0/a1 to their home slots at
 * entry and copies both 32-bit values into base_x/base_y together. The frame reserves
 * 40 bytes of locals the body never reads. */
void wldcore_window_build_yes_no_panel(point32_t origin) {
    RECT bounds;
    s32 dead_locals[10];
    s32 index;
    s32 depth;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.header_window = index;
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
    g_wldcore_window_render_records[index].base = origin;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].choice_panel.row = 0;
    g_wldcore_menu_stack_records_next[depth].choice_panel.mode = WLDCORE_ACCEPT_CONFIRM_MODE_CHOOSE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_menu_stack_records_next[depth].choice_panel.row * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
}

/* Input step of the proposition yes/no confirmation panel.
 *
 * Up/down toggle the highlighted row and move the cursor window by one
 * 16-pixel row. Cancel (0x40) and "no" both tear the panel down and return
 * through wldcore_list_open_available_propositions. "Yes" first checks that at least one non-guest,
 * non-monster, generic party unit is free (the same filter as
 * wldcore_proposition_load_send_unit_candidates) and that script variable 0x2c
 * is at least g_wldcore_proposition_gil_amount; failing either shows message 0xb822 or 0xb811 and
 * parks the level in mode 1 or 3 waiting for a button, and mode 1 then chains
 * message 0xb8e0 before mode 3 closes. A successful confirmation continues
 * into wldcore_proposition_push_send_unit_level.
 *
 * The gotos are the target's shared tails: the mode 3 close, the "no"
 * dismissal and the no-unit failure jump into the cancel and gil-failure
 * blocks; duplicated tails do not cross-jump back into that shape. */
void wldcore_proposition_handle_accept_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 available;
    s32 i;
    s32 row;
    party_unit_t* unit;

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->mode == WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        goto close;
    }
    if (level->mode == WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_HUMAN_UNIT, 0);
            level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE;
        }
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    dismiss:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 3;
    close:
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth -= 2;
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
    if (!(g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->row == 1) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        goto dismiss;
    }
    available = 0;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        unit = wldcore_get_party_unit(i);
        if (unit->proposition_status == 0 && unit->party_id != PARTY_ID_NONE
            && !(unit->gender_flags & UNIT_FLAG_MONSTER) && unit->sprite_set >= CHARACTER_IDENTITY_BOY) {
            available++;
        }
    }
    if (available == 0) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_MORE_UNITS, 0);
        level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE;
        goto fail;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < g_wldcore_proposition_gil_amount) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NOT_ENOUGH_GIL, 0);
        level->mode = WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE;
    fail:
        wldcore_sound_play_effect(MAIN_SFX_INVALID);
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count -= 3;
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    g_wldcore_window_record_count -= 2;
    g_wldcore_window_render_record_count--;
    g_wldcore_window_render_object_count -= 3;
    g_wldcore_window_render_record_count--;
    g_wldcore_window_render_object_count--;
    g_wldcore_menu_stack_depth -= 2;
    wldcore_proposition_push_send_unit_level();
}

/* Pushes the "how many days" number panel of a proposition: shows prompt
 * 0xB813 with the row's minimum and maximum days, starts the number panel
 * thread on that range starting at the minimum, and withholds Circle and
 * Cross from the WORLD menu threads (the level's own step handles them). */
void wldcore_menu_push_number_panel_level(void) {
    point32_t origin;
    s32 record_index;
    s32 x;
    s32 y;

    x = g_wldcore_selected_proposition_row[0].min_days;
    y = g_wldcore_selected_proposition_row[0].max_days;
    g_text_substitution_values[0] = x;
    g_text_substitution_values[1] = y;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_DAYS, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 8;
    origin.x = 0x38;
    origin.y = 0x20;
    g_wldcore_active_menu_value = x;
    wldcore_window_init_number_panel_render_thread(
        WLDCORE_THREAD_SLOT_WINDOW_PANEL, x | (y << 16), WLDCORE_TEXT_ID_DAYS_LABEL, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Input step of a number-panel level that waits on WORLD thread 12: once the
 * thread finishes, the pending result (level->result) selects which resume
 * handler runs and two levels are popped. Otherwise cancel arms result -1 and
 * confirm arms result 1, publishing the entered number, and either restarts
 * thread 12. */
void wldcore_menu_number_panel_wait_for_thread_step(wldcore_menu_panel_level_t* level) {
    if (level->result != 0 && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_menu_stack_depth -= 2;
        if (level->result == 1) {
            wldcore_proposition_push_dispatch_confirm_level();
        } else {
            wldcore_list_open_available_propositions();
        }
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_FUNDS;
        level->result = -1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_wldcore_proposition_dispatch_days = g_wldcore_active_menu_value;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    }
}

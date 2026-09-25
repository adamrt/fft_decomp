#include "fft/wldcore.h"
#include "psx/pad.h"

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
 * The volatile unit pointer keeps reorg from hoisting the loop increment into
 * the filter branches' delay slots, which the target leaves as nops.
 *
 * The gotos are the target's shared tails: the mode 3 close, the "no"
 * dismissal and the no-unit failure jump into the cancel and gil-failure
 * blocks; duplicated tails do not cross-jump back into that shape. */
void wldcore_proposition_handle_accept_confirm_input(wldcore_menu_choice_panel_level_t* level) {
    s32 available;
    s32 i;
    s32 render_index;
    s32 window;
    s32 row;
    s32 row_y;
    s32 y;
    volatile party_data_t* unit;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (level->mode == 3) {
        if (!(g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        goto close;
    }
    if (level->mode == 1) {
        if (g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            world_thread_set_parameters(0xE, 0x19, 0xB8E0, 0);
            level->mode = 3;
        }
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
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
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0
        || wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        row = level->row;
        row = row ^ 1;
        window = level->cursor_window;
        render_index = level->render_index;
        level->row = row;
        g_wldcore_window_records[window].y
            = (y = g_wldcore_window_render_records[render_index].base_y) + (row_y = row * 16 + 14);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (!(g_wldcore_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->row == 1) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        goto dismiss;
    }
    available = 0;
    for (i = 0; i < PARTY_GUEST_SLOT_FIRST; i++) {
        unit = wldcore_get_party_data_pointer(i);
        if (unit->proposition_status == 0 && unit->party_id != PARTY_ID_NONE
            && !(unit->gender_flags & UNIT_FLAG_MONSTER) && unit->sprite_set >= CHARACTER_IDENTITY_BOY) {
            available++;
        }
    }
    if (available == 0) {
        world_thread_set_parameters(0xE, 0x19, 0xB822, 0);
        level->mode = 1;
        goto fail;
    }
    if (world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) < g_wldcore_proposition_gil_amount) {
        world_thread_set_parameters(0xE, 0x19, 0xB811, 0);
        level->mode = 3;
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

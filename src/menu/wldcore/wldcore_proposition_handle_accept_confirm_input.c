#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

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

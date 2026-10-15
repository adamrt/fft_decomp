/* Brightness, choice delta, fade wait and operand history. */

#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_menu_brightness(void) {
    world_menu_set_brightness((u8)(g_wldcore_active_saved_record.instruction >> 8),
        (u8)(g_wldcore_active_saved_record.instruction >> 16), (u8)(g_wldcore_active_saved_record.instruction >> 24));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Script opcode handler: compares `EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION` against the opcode's
 * third byte and, on a match, adds the signed middle 16 bits of the
 * 32-bit instruction to g_wldcore_sound_novel_counter_delta. */

/* Scalar counter access preserves the separate load/store symbol bases. */
extern u16 g_wldcore_sound_novel_counter_delta;

void wldcore_opcode_add_counter_delta_if_choice(void) {
    u32* instruction;
    u8 expected;

    instruction = &g_wldcore_opcode_state.instruction;

    expected = *instruction >> 24;
    if (script_get_variable(EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION) == expected) {
        g_wldcore_sound_novel_counter_delta += (s32)((*instruction << 8) >> 16) - 0x8000;
    }
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: yield while a screen fade is running. */
void wldcore_opcode_wait_for_fade(void) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) == 0) {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_YIELD;
    }
}

/* Shift the four-entry history g_wldcore_sound_novel_text_history_0..66 down one slot and push
 * operand_0 at the end (same shape as
 * wldcore_opcode_start_script_thread_and_push_history). */
/* Target 0x8008cb30. */
void wldcore_opcode_push_operand_history(void) {
    u16 history_1;
    u8 new_entry;
    u16 history_2;
    u16 history_3;

    history_1 = g_wldcore_sound_novel_text_history_1;
    new_entry = (g_wldcore_active_saved_record.instruction & 0xff00) >> 8;
    history_2 = g_wldcore_sound_novel_text_history_2;
    history_3 = g_wldcore_sound_novel_text_history_3;
    g_wldcore_sound_novel_text_history_0 = history_1;
    g_wldcore_sound_novel_text_history_1 = history_2;
    g_wldcore_sound_novel_text_history_2 = history_3;
    g_wldcore_sound_novel_text_history_3 = new_entry;
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

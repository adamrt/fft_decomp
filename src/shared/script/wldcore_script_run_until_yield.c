#include "fft/script.h"
#include "fft/wldcore.h"

/* Runs the world-script interpreter until the active record yields. Each turn
 * refreshes the pending script sounds from script variables 0x5c-0x5e, then:
 *
 *   flag 0x02 (branch armed) marks the branch's saved-data bit, advances to the
 *   next section, repoints the 32-bit instructions at that section's offset in
 *   the save-slot buffer, and - for the first five maps - publishes the whole
 *   record into g_main_saved_records[g_wldcore_saved_record_index];
 *
 *   flag 0x04 (fetch pending) loads the next 32-bit instruction and advances the
 *   32-bit element index, arming flag 0x10;
 *
 *   the opcode byte then selects a handler from g_wldcore_opcode_handlers.
 *
 * The turn loop ends once the handler leaves flag 0x08 (yield) or 0x20 (abort)
 * set. Returns zero when the script aborted, which is how the caller at
 * 0x8007c2d0 tests it; it also clears the yield flag on the way out.
 *
 * The record holds the loaded script's section-offset table. */
s32 wldcore_script_run_until_yield(void) {
    s32 low;
    s32 high;
    u32 section_byte_offset;
    u16 flags;
    s32 aborted;

    do {
        low = script_get_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT);
        high = script_get_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND);
        wldcore_sound_play_pending_script_sounds(low, high, script_get_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK));
        wldcore_sound_novel_update_countdown_timer();

        if (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_BRANCH) {
            wldcore_set_bit_value(
                g_main_saved_data_bits, (g_wldcore_saved_record_index << 8) + g_wldcore_active_saved_record.section, 1);
            g_wldcore_active_saved_record.state_flags &= ~WLDCORE_OPCODE_STATE_BRANCH;
            section_byte_offset = g_wldcore_active_saved_record.buffer->offsets[g_wldcore_active_saved_record.section];
            g_wldcore_active_saved_record.section++;
            g_wldcore_active_saved_record.word_index = 0;
            g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
            g_wldcore_active_saved_record.script_words
                = &((u32*)&g_main_save_slot_buffer)[section_byte_offset / sizeof(u32)];
            if (g_wldcore_saved_record_index < 5) {
                memcpy(&g_main_saved_records[g_wldcore_saved_record_index], &g_wldcore_active_saved_record,
                    sizeof(wldcore_saved_record_t));
            }
        }

        if (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_FETCH) {
            g_wldcore_active_saved_record.state_flags
                = (g_wldcore_active_saved_record.state_flags & ~WLDCORE_OPCODE_STATE_FETCH)
                | WLDCORE_OPCODE_STATE_FETCHED;
            g_wldcore_active_saved_record.instruction
                = g_wldcore_active_saved_record.script_words[g_wldcore_active_saved_record.word_index];
            g_wldcore_active_saved_record.word_index++;
        }

        g_wldcore_opcode_handlers[(u8)g_wldcore_active_saved_record.instruction]();

        flags = g_wldcore_active_saved_record.state_flags;
        g_wldcore_active_saved_record.state_flags = flags & ~WLDCORE_OPCODE_STATE_FETCHED;
    } while ((flags & (WLDCORE_OPCODE_STATE_YIELD | WLDCORE_OPCODE_STATE_END)) == 0);

    aborted = g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_END;
    g_wldcore_active_saved_record.state_flags &= ~WLDCORE_OPCODE_STATE_YIELD;
    return aborted == 0;
}

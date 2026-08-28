#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* The live world-script record at 0x800d4840. It has the same 0x38-byte layout
 * as the g_main_saved_records slots it is copied into, so the copy below is a
 * whole-record assignment. The scalar names the tree already uses for its
 * interior (g_wldcore_script_slot_table, g_wldcore_active_saved_record.script_words, g_wldcore_opcode_state,
 * g_wldcore_active_saved_record.state_flags, g_wldcore_active_saved_record.section) are the same storage seen field by
 * field. */
typedef struct wldcore_world_script_record {
    wldcore_save_slot_table_t* slot_table; /* 0x00; g_wldcore_script_slot_table */
    u32* words;                            /* 0x04; g_wldcore_active_saved_record.script_words */
    union {
        u32 word; /* 0x08; g_wldcore_opcode_state */
        wldcore_opcode_instruction_bytes_t bytes;
    } instruction;
    u16 flags;           /* 0x0c; g_wldcore_active_saved_record.state_flags */
    s16 section;         /* 0x0e; g_wldcore_active_saved_record.section */
    s16 word_index;      /* 0x10 */
    u8 unknown_12[0x26]; /* 0x12; the rest of the 0x38-byte record */
} wldcore_world_script_record_t;

typedef char wldcore_world_script_record_size_must_be_0x38[sizeof(wldcore_world_script_record_t) == 0x38 ? 1 : -1];

/* The same storage seen from its flags field onwards. The target names exactly
 * one constant, 0x800d484c, and reaches flags/section/word_index at 0/2/4 off
 * it and the two earlier fields at -8/-4; declaring a type whose field zero IS
 * flags is what puts the bias in the type instead of in pointer arithmetic. */
typedef struct wldcore_world_script_state {
    u16 flags;      /* 0x0c; g_wldcore_active_saved_record.state_flags */
    s16 section;    /* 0x0e; g_wldcore_active_saved_record.section */
    s16 word_index; /* 0x10 */
} wldcore_world_script_state_t;

/* The record seen from the same base. Spelled per use rather than held in a
 * local: a shared pointer local becomes its own pseudo and is allocated a
 * second callee-saved register holding 0x800d4840, where the target folds the
 * bias into a displacement off the single 0x800d484c base. */
#define WLDCORE_RECORD(state) ((wldcore_world_script_record_t*)((u8*)(state) - 12))

void wldcore_sound_play_pending_script_sounds(s32 low, s32 high, s32 value);
s32 wldcore_sound_novel_update_countdown_timer(void);

/* Runs the world-script interpreter until the active record yields. Each turn
 * refreshes the pending script sounds from script variables 0x5c-0x5e, then:
 *
 *   flag 0x02 (branch armed) marks the branch's saved-data bit, advances to the
 *   next section, repoints the instruction words at that section's offset in
 *   the save-slot buffer, and - for the first five maps - publishes the whole
 *   record into g_main_saved_records[g_wldcore_saved_record_index];
 *
 *   flag 0x04 (fetch pending) loads the next instruction word and advances the
 *   word index, arming flag 0x10;
 *
 *   the opcode byte then selects a handler from g_wldcore_opcode_handlers.
 *
 * The turn loop ends once the handler leaves flag 0x08 (yield) or 0x20 (abort)
 * set. Returns zero when the script aborted, which is how the caller at
 * 0x8007c2d0 tests it; it also clears the yield flag on the way out.
 *
 * Addressing notes, all read off the target:
 *  - the base register holds 0x800d484c, the address of the flags field, and
 *    is formed once in the prologue; the interior fields are reached at 0/2/4
 *    and the word pointer and instruction word at -8/-4, with the record copy
 *    sourced from addiu a2,s2,-12. The state type above supplies that bias, so
 *    the record pointer derived from it carries no symbol of its own;
 *  - the two reads of the slot table and of the instruction words are
 *    absolute, so those two sites keep the tree's bare extern spellings;
 *  - the flag and section reads in the 0x02 arm are re-read after the
 *    wldcore_set_bit_value call rather than held across it;
 *  - the section and word-index locals are s32, not s16: a sub-word local is a
 *    truncation pseudo and emits a load-and-shift pair where the target has a
 *    bare signed load.
 *
 * Remaining levers, each placed from the target:
 *  - the tail reloads the flags through a fresh `u16*` to the field, which
 *    reproduces the target's rematerialised base at offset zero;
 *  - `aborted` masks the flags before the `& ~8` store, so the return takes
 *    GCC's shift path (andi/sltiu) rather than a generic comparison;
 *  - the empty barriers in the 0x02 arm and the fetch arm split scheduling
 *    regions so the stores and the words chain keep the target's order;
 *  - the words offset is built in three assignments (shift, table add,
 *    load) so the table load follows the shift, as at 0x9c-0xb4;
 *  - an unreferenced 16-byte local supplies the frame's `vars` area. */
s32 wldcore_script_run_until_yield(void) {
    /* The target's frame is 56 bytes: a 16-byte outgoing-argument area, 16
     * bytes of `vars`, and the five register saves at sp+32..sp+48. Nothing in
     * the body touches the middle 16 bytes, so the original declared a local
     * there that it never read. An unreferenced array reproduces it at zero
     * instruction cost. */
    u8 unused_scratch[16];
    wldcore_world_script_state_t* state;
    s32 low;
    s32 high;
    u16 flags;

    state = (wldcore_world_script_state_t*)&g_wldcore_active_saved_record.state_flags;

    do {
        low = world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT);
        high = world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND);
        wldcore_sound_play_pending_script_sounds(
            low, high, world_script_get_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK));
        wldcore_sound_novel_update_countdown_timer();

        if (state->flags & 2) {
            s32 section;
            s32 armed;
            u32 offset;
            /* Pin: unpinned, the table pointer and section swap $a0/$v1 and the guard read moves. */
            register s32 table __asm__("$4");

            wldcore_set_bit_value(g_main_saved_data_bits, (g_wldcore_saved_record_index << 8) + state->section, 1);
            armed = state->flags & ~2;
            section = state->section;
            offset = section * 4;
            state->flags = armed;
            /* Keeps the flags update ahead of the table load instead of interleaving them. */
            __asm__ volatile("");
            table = (s32)(u8*)g_wldcore_script_slot_table;
            offset = offset + table;
            offset = ((wldcore_save_slot_table_t*)offset)->offsets[0];
            state->section = section + 1;
            state->word_index = 0;
            state->flags = armed | 4;
            WLDCORE_RECORD(state)->words = &((u32*)g_main_save_slot_buffer)[offset >> 2];
            if (g_wldcore_saved_record_index < 5) {
                wldcore_saved_record_t* records = g_main_saved_records;

                records[g_wldcore_saved_record_index] = *(wldcore_saved_record_t*)WLDCORE_RECORD(state);
            }
        }

        if (state->flags & 4) {
            s32 word_index = state->word_index;

            state->flags = (state->flags & ~4) | 0x10;
            /* Keeps the words load below the flags store. */
            __asm__ volatile("");
            WLDCORE_RECORD(state)->instruction.word = g_wldcore_active_saved_record.script_words[word_index];
            state->word_index = word_index + 1;
        }

        g_wldcore_opcode_handlers[WLDCORE_RECORD(state)->instruction.bytes.opcode]();

        flags = state->flags;
        state->flags = flags & ~0x10;
    } while ((flags & 0x28) == 0);

    {
        u16* flags_p = &g_wldcore_active_saved_record.state_flags;
        u16 final_flags = *flags_p;
        s32 aborted = final_flags & 0x20;

        *flags_p = final_flags & ~8;
        return aborted == 0;
    }
}

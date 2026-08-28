#include "fft/battle.h"
#include "fft/event.h"
#include "psx/types.h"

/* Event instructions are an unaligned byte stream; operands are read bytewise. */
#define EVENT_BYTE(offset, index) (((u8*)g_battle_event_block + (offset))[index])

/* Measure the widest text a DisplayMessage window will show.
 *
 * With flag 0x80 set, the message at `offset` is measured, then every later
 * ChangeDialog (0x51) whose window mask (operand byte 1) covers `flags & 3`
 * contributes its text width, until a ChangeDialog with text id 0xffff closes
 * that window. The result is the widest measured text. */
void battle_text_build_display_message(s32 offset, s32 flags, s32* out_width) {
    s16 width;
    s16 height;
    s32 max_width;
    s32 next;
    s32 size;

    if (flags & 0x80) {
        flags &= 3;
        battle_text_measure_pixels(
            &width, &height, battle_text_init_entry(((EVENT_BYTE(offset, 4) << 8) + EVENT_BYTE(offset, 3)) - 1));
        max_width = width;
        next = offset + 1;
        size = g_battle_script_event_instruction_sizes[EVENT_OPCODE_DISPLAY_MESSAGE];
        offset = next + size;
        while ((offset = battle_script_find_instruction_byte_offset(offset, EVENT_OPCODE_CHANGE_DIALOG)) != 0) {
            if (EVENT_BYTE(offset, 0) == EVENT_OPCODE_CHANGE_DIALOG) {
                if (EVENT_BYTE(offset, 2) == 0xFF && EVENT_BYTE(offset, 3) == 0xFF
                    && (EVENT_BYTE(offset, 1) & flags) == flags) {
                    break;
                }
                if ((EVENT_BYTE(offset, 1) & flags) == flags) {
                    battle_text_measure_pixels(&width, &height,
                        battle_text_init_entry(((EVENT_BYTE(offset, 3) << 8) + EVENT_BYTE(offset, 2)) - 1));
                    if (max_width < width) {
                        max_width = width;
                    }
                }
            }
            next = offset + 1;
            size = g_battle_script_event_instruction_sizes[EVENT_OPCODE_CHANGE_DIALOG];
            offset = next + size;
        }
        *out_width = max_width;
    }
}

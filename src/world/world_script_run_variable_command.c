#include "fft/event.h"
#include "fft/script_variables.h"
#include "fft/world.h"

/* WORLD 0x800fd4a8..0x800fd7c4. EVENT_SCRIPT_VAR_RANDOM_VALUE is read through the event
 * RNG, not its ordinary storage. ZERO never reads the uninitialized operand.
 * Keep the target's arithmetic edge behavior: only division explicitly stops
 * the thread on zero; the assembler emits checked division/modulo traps. */
void world_script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused) {
    s32* source_word;
    s32* destination_word;
    s32 write_allowed;
    s32 source_bit;
    s32 destination_bit;
    u32 shifted_mask;
    u32 mask;
    s32 operand;
    u32 preserve_mask;
    u32 replay_mode;
    u32 value;

    if (opcode != EVENT_OPCODE_ZERO_VARIABLE) {
        if (!(opcode & 1)) {
            operand = source;
        } else if (source == EVENT_SCRIPT_VAR_RANDOM_VALUE) {
            operand = world_script_get_random_u16();
        } else {
            source_word = world_script_get_variable_word(source);
            source_bit = world_get_script_variable_bit_position(source);
            if (source_bit >= 0) {
                mask = 1;
                if (source >= EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
                    mask = 0xf;
                }
                operand = (s32)((u32)((mask << source_bit) & *source_word) >> source_bit);
            } else {
                operand = *source_word;
            }
        }
    }
    preserve_mask = -1;
    destination_word = world_script_get_variable_word(destination_id);
    destination_bit = world_get_script_variable_bit_position(destination_id);
    if (destination_bit >= 0) {
        mask = 1;
        if (destination_id >= EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            mask = 0xf;
        }
        shifted_mask = mask << destination_bit;
        preserve_mask = ~shifted_mask;
        value = (u32)(shifted_mask & *destination_word) >> destination_bit;
    } else {
        value = (u32)*destination_word;
    }
    if (opcode == EVENT_OPCODE_ZERO_VARIABLE) {
        value = 0;
    } else if (opcode < EVENT_OPCODE_SUBTRACT_IMMEDIATE) {
        value += operand;
    } else if (opcode < EVENT_OPCODE_MULTIPLY_IMMEDIATE) {
        value -= operand;
    } else if (opcode < EVENT_OPCODE_DIVIDE_IMMEDIATE) {
        value *= operand;
    } else if (opcode < EVENT_OPCODE_MODULO_IMMEDIATE) {
        if (operand == 0) {
            world_thread_exit_current();
        }
        value = (u32)((s32)value / operand);
    } else if (opcode < EVENT_OPCODE_AND_IMMEDIATE) {
        value = (u32)((s32)value % operand);
    } else if (opcode < EVENT_OPCODE_OR_IMMEDIATE) {
        value &= operand;
    } else if (opcode < EVENT_OPCODE_ZERO_VARIABLE) {
        value |= operand;
    }
    /* Guard recursive getter calls: replay mode and saturated high variables
     * can suppress writes, but the nested getter must still read its operand. */
    if (g_world_event_variable_write_guard == 0) {
        if (destination_id >= 0x3c0) {
            g_world_event_variable_write_guard = 1;
            write_allowed = (u32)world_script_get_variable(destination_id) < 12;
            g_world_event_variable_write_guard = 0;
            if (!write_allowed) {
                return;
            }
        }
        if (g_world_event_variable_write_guard == 0) {
            g_world_event_variable_write_guard = 1;
            replay_mode = world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS);
            g_world_event_variable_write_guard = 0;
            if (replay_mode != 0 && (u32)(destination_id - 0x70) >= 0x20U
                && (u32)(destination_id - EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) >= 4U && destination_id != 0x66
                && (u32)(destination_id - 0x32) >= 8U && destination_id >= 0x2c && destination_id != 0x30
                && (u32)(destination_id - 0x56) >= 5U && destination_id != 0x53) {
                return;
            }
        }
    }
    if (destination_bit >= 0) {
        mask = 1;
        if (destination_id >= EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            mask = 0xf;
        }
        value &= mask;
        value <<= destination_bit;
        *destination_word = value | (preserve_mask & *destination_word);
        return;
    }
    *destination_word = (s32)value;
    if (destination_id == 0x19) {
        g_world_text_substitution_values[0] = value;
    }
}

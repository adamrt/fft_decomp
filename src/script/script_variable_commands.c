/* Jump targets, conditions, variable commands, waits and the event RNG. */

#include "fft/battle.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libetc.h"

s32 script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id) {
    u8* base;
    s32 offset;
    s32 found;
    s32 opcode;

    found = -1;
    if (target_opcode == EVENT_OPCODE_BACK_TARGET) {
        for (offset = 0; offset < limit_offset; offset += 1 + g_script_event_instruction_sizes[opcode]) {
            base = (u8*)g_script_event_block;
            opcode = base[offset];
            if (opcode == EVENT_OPCODE_BACK_TARGET_BARRIER
                || (opcode == EVENT_OPCODE_FORWARD_TARGET && (base + offset)[1] == target_id)) {
                found = -1;
            } else if (opcode == target_opcode && (base + offset)[1] == target_id) {
                found = offset;
            }
        }
        if (found >= 0) {
            return found + 2;
        }
    } else {
        while ((opcode = ((u8*)g_script_event_block)[limit_offset]) != EVENT_OPCODE_END) {
            if (opcode == target_opcode && ((u8*)g_script_event_block + limit_offset)[1] == target_id) {
                return limit_offset + 2;
            }
            if (alternate_opcode != -1 && opcode == alternate_opcode
                && ((u8*)g_script_event_block + limit_offset)[1] == target_id) {
                return limit_offset + 2;
            }
            limit_offset += 1 + g_script_event_instruction_sizes[opcode];
        }
    }
    /* Does not return; the target has no return value on this path. */
    thread_exit_current();
}

/* Returns the byte offset of the next `opcode` instruction at or after
 * `offset`, or 0 when EventEnd comes first. */
s32 script_find_instruction_byte_offset(s32 offset, s32 opcode) {
    /* Bytecode offsets are relative to the complete file block. */
    u8* base = (u8*)g_script_event_block;
    s32 operands_offset;
    u8 op;
    for (;;) {
        op = base[offset];
        if (op == EVENT_OPCODE_END)
            return 0;
        if (op == opcode)
            return offset;
        operands_offset = offset + 1;
        offset = operands_offset + g_script_event_instruction_sizes[op];
    }
}

/* Comparison opcodes 0xa0..0xa5: replace the comparison-result variable (32-bit script
 * variable 0) with the truth value of comparing it against the operand variable (variable
 * 1), using <=, >=, ==, !=, < and > in opcode order. Any other opcode changes
 * nothing. */
void script_run_condition(event_opcode_e opcode) {
    if (opcode == EVENT_CONDITION_SECOND_GTE_FIRST) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            <= g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_CONDITION_FIRST_GTE_SECOND) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            >= g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_CONDITION_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            == g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_CONDITION_NOT_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            != g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_CONDITION_FIRST_LT_SECOND) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            < g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_CONDITION_SECOND_LT_FIRST) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            > g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
}

/* EVENT_SCRIPT_VAR_RANDOM_VALUE is read through the event
 * RNG, not its ordinary storage. ZERO never reads the uninitialized operand.
 * Keep the target's arithmetic edge behavior: only division explicitly stops
 * on zero; modulo has no such check, and signed overflow is not normalized. */
void script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused) {
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
    s32 value;

    if (opcode != EVENT_OPCODE_ZERO_VARIABLE) {
        if (!(opcode & 1)) {
            operand = source;
        } else if (source == EVENT_SCRIPT_VAR_RANDOM_VALUE) {
            operand = script_get_rand16();
        } else {
            source_word = script_get_variable_word_pointer_from_id(source);
            source_bit = script_get_variable_bit_position(source);
            if (source_bit >= 0) {
                mask = 1;
                if (source >= EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
                    mask = 0xf;
                }
                operand = ((mask << source_bit) & *source_word) >> source_bit;
            } else {
                operand = *source_word;
            }
        }
    }
    preserve_mask = -1;
    destination_word = script_get_variable_word_pointer_from_id(destination_id);
    destination_bit = script_get_variable_bit_position(destination_id);
    if (destination_bit >= 0) {
        mask = 1;
        if (destination_id >= EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            mask = 0xf;
        }
        shifted_mask = mask << destination_bit;
        preserve_mask = ~shifted_mask;
        value = (shifted_mask & *destination_word) >> destination_bit;
    } else {
        value = *destination_word;
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
            thread_exit_current();
        }
        value = value / operand;
    } else if (opcode < EVENT_OPCODE_AND_IMMEDIATE) {
        value = value % operand;
    } else if (opcode < EVENT_OPCODE_OR_IMMEDIATE) {
        value &= operand;
    } else if (opcode < EVENT_OPCODE_ZERO_VARIABLE) {
        value |= operand;
    }
    /* Guard recursive getter calls: replay mode and saturated high variables
     * can suppress writes, but the nested getter must still read its operand. */
    if (g_script_variable_write_guard == 0) {
        if (destination_id >= EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST) {
            g_script_variable_write_guard = 1;
            write_allowed = (u32)script_get_variable(destination_id) < 12;
            g_script_variable_write_guard = 0;
            if (!write_allowed) {
                return;
            }
        }
        if (g_script_variable_write_guard == 0) {
            g_script_variable_write_guard = 1;
            replay_mode = script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS);
            g_script_variable_write_guard = 0;
            if (replay_mode != 0 && (destination_id - EVENT_SCRIPT_VAR_SCENARIO_LOCAL_FIRST) >= 0x20U
                && (destination_id - EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) >= 4U
                && destination_id != EVENT_SCRIPT_VAR_TEXT_STORE_INDEX
                && (destination_id - EVENT_SCRIPT_VAR_CURRENT_ENTD) >= 8U
                && destination_id >= EVENT_SCRIPT_VAR_WAR_FUNDS && destination_id != EVENT_SCRIPT_VAR_MAP_ARRANGEMENT
                && (destination_id - EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE) >= 5U
                && destination_id != EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE) {
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
    *destination_word = value;
    if (destination_id == EVENT_SCRIPT_VAR_TEXT_SUBSTITUTION_VALUE) {
        g_text_substitution_values[0] = value;
    }
}

/* Resolves a script variable id to the 32-bit storage value that holds it. Ids below
 * 0x80 own a whole 32-bit value; 0x80..0x35F pack 32 flags per 32-bit value starting at element
 * 0x80, and 0x360..0x3FF pack eight nibbles per 32-bit value starting at element 0x97.
 * Ids at or above 0x400 stop the calling thread; the result is unset if it returns. */
s32* script_get_variable_word_pointer_from_id(s32 variable_id) {
    s32* word;

    if (variable_id < EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) {
        word = &g_script_variables[variable_id];
    } else if (variable_id < EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
        word = &g_script_variables[EVENT_SCRIPT_PACKED_BIT_WORD_FIRST
            + ((variable_id - EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) / 32)];
    } else if (variable_id < EVENT_SCRIPT_VAR_END) {
        word = &g_script_variables[EVENT_SCRIPT_PACKED_NIBBLE_WORD_FIRST
            + ((variable_id - EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) / 8)];
    } else {
        thread_exit_current();
    }
    return word;
}

/* Return the packed variable field position, or -1 for a full word; invalid ids exit the thread. */
s32 script_get_variable_bit_position(s32 value) {
    s32 result;

    result = -1;
    if (value >= EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) {
        if (value < EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            result = value & 0x1f;
        } else if (value < EVENT_SCRIPT_VAR_END) {
            result = (value & 7) * 4;
        } else {
            thread_exit_current();
        }
    }
    return result;
}

/* Wait until the variable reaches the signed 16-bit test value. */
void script_wait_value_event_instruction(const event_wait_value_parameters_t* parameters) {
    s32 variable_id;
    s32 test_value;

    variable_id = script_read_s16(parameters->variable_id_le);
    test_value = script_read_s16(parameters->test_value_le);
    while (script_get_variable(variable_id) < test_value) {
        thread_yield();
    }
}

/* Seed the event script's 16-bit random generator (script_get_rand16) with
 * the vertical-blank count. */
void script_seed_random_from_vsync(void) {
    g_script_rand16_state = VSync(-1);
}

/* Step the event script's random generator, state * 0x6255 + 0x3619 kept to 16
 * bits, and return the new state. script_run_variable_command calls it
 * when a variable opcode reads EVENT_SCRIPT_VAR_RANDOM_VALUE. */
s32 script_get_rand16(void) {
    return g_script_rand16_state = (g_script_rand16_state * 0x6255U + 0x3619U) & 0xffffU;
}

/* Yield while the unit-ready test returns zero. */
void script_wait_for_unit_ready(s32 runtime_id) {
    while (battle_return_one(runtime_id) == 0) {
        thread_yield();
    }
}

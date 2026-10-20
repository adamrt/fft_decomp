/* Event task selection, branch searches and variable commands. The instruction
 * lengths support scanners as well as the interpreter, while task IDs coordinate
 * the cooperative threads started by event instructions. */

#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_runtime.h"
#include "psx/libetc.h"
#include "psx/types.h"

extern s32 g_script_rand16_state;

/* Suppress recursive write filtering while getters use comparison variable 0. */
extern u8 g_script_variable_write_guard;

/* Operand-byte counts for opcodes 0x00 through 0xf3. The opcode byte is additional. */
/* clang-format off */
u8 g_script_event_instruction_sizes[0xf4] = {
    /* 0x00 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 0x10 */ 14,  5,  2,  2,  2,  0,  0,  5,  6, 16,  5, 14,  1,  0,  0,  5,
    /* 0x20 */  6,  2,  3,  3,  0,  4,  2,  0,  8,  2,  0,  0,  7,  6,  8,  0,
    /* 0x30 */  0,  5,  7,  5,  2,  4,  2,  2,  2,  0,  0, 12,  2,  2,  9,  4,
    /* 0x40 */  5,  4,  0,  1,  2,  3,  2,  8,  0,  0,  0,  0,  2,  1,  3,  1,
    /* 0x50 */  1,  5,  2,  7,  2,  2,  0,  0,  3,  1,  1,  1,  3,  1,  1,  6,
    /* 0x60 */  2,  3,  6,  1,  2,  0,  0,  2,  3,  8,  5,  5,  2,  2, 12,  2,
    /* 0x70 */  4,  2,  0, 14,  6,  6,  6,  0,  2,  4,  2,  2,  0,  1,  4,  4,
    /* 0x80 */  3,  3,  0,  5,  1,  1,  3,  4,  0,  0,  0,  0,  6,  0,  0,  1,
    /* 0x90 */  3,  3,  5,  2,  2,  0,  0,  2,  2,  2,  0,  0,  0,  0,  0,  0,
    /* 0xa0 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 0xb0 */  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  0,
    /* 0xc0 */ 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 0xd0 */  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  0,  0,  0,  0,  0,  0,
    /* 0xe0 */  2,  1,  1,  0,  1,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 0xf0 */  0,  2,  0,  0,
};

/* clang-format on */

static s32 random16_next(void);

static s32 variable_shift_get(s32 value);

static s32* variable_word_get(s32 variable_id);

/* Event instruction 0x37 (unused by the scenario scripts): toggles bit 0 of
 * portrait_flipped of the menu window buffer whose message_id matches the
 * operand.
 *
 * record_index is never assigned: the target indexes with whatever $s0 holds
 * on entry and tests that one record six times. */
void script_opcode_37_flip_message_portrait(const u8* parameters) {
    s32 i;
    s32 record_index;
    s16 record_id = script_s16_read(parameters);
    menu_window_buffer_t* record;

    for (i = 0; i < 6; i++) {
        if (g_menu_window_buffers[record_index].message_id == record_id) {
            record = &g_menu_window_buffers[record_index];
            record->portrait_flipped = g_menu_window_buffers[record_index].portrait_flipped + 1;
            g_menu_window_buffers[record_index].portrait_flipped
                = g_menu_window_buffers[record_index].portrait_flipped & 1;
        }
    }
}

/* EVENT_OPCODE_UNKNOWN_74: receives the operand pointer and does nothing. */
void script_opcode_74_unknown(const u8* unused_parameters) {
}

/* Returns the slot a thread should start in: a requested id below 16 as it is,
 * otherwise (THREAD_SLOT_FIRST_IDLE) the first idle slot from 1 up. With
 * every slot busy the calling thread exits, and the function has no return value
 * for that path. */
s32 thread_slot_resolve(s32 requested_thread_id) {
    s32 i;

    if (requested_thread_id < THREAD_ID_COUNT) {
        return requested_thread_id;
    }
    i = 1;
    do {
        if (thread_is_running_asm(i) == 0) {
            return i;
        }
        i++;
    } while (i < VARIANT_THREAD_TABLE_SLOT_COUNT);
    thread_current_exit();
}

/* As thread_slot_resolve, but the search for an idle slot starts above the
 * current thread's slot. */
s32 thread_next_slot_resolve(s32 thread_id) {
    s32 index;

    if (thread_id < THREAD_ID_COUNT) {
        return thread_id;
    }

    index = g_thread_current_id + 1;
    if (index >= VARIANT_THREAD_TABLE_SLOT_COUNT) {
        thread_current_exit();
    } else {
        do {
            if (thread_is_running_asm(index) == 0) {
                return index;
            }
            index++;
        } while (index < VARIANT_THREAD_TABLE_SLOT_COUNT);

        thread_current_exit();
    }
}

/* Return the id of another running thread whose 32-bit current task value equals
 * task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 thread_task_find(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < VARIANT_THREAD_TABLE_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}

/* Tag the running thread with task_id, the value thread_task_find looks up. */
void thread_current_task_set(s32 task_id) {
    g_thread_contexts[g_thread_current_id].task_id = task_id;
}

/* Find a jump's target and return the offset after its two bytes. For BackTarget it scans from the
 * event start to limit_offset and keeps the last matching BackTarget not followed by a barrier
 * (0xd9) or a same-id ForwardTarget; otherwise it scans forward from limit_offset to EventEnd for
 * target_opcode or alternate_opcode. With no target the thread exits. */
s32 script_jump_target_find(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id) {
    u8* base;
    s32 offset;
    s32 found = -1;
    s32 opcode;

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
    thread_current_exit();
}

/* Returns the byte offset of the next `opcode` instruction at or after
 * `offset`, or 0 when EventEnd comes first. */
s32 script_instruction_offset_find(s32 offset, s32 opcode) {
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
void script_condition_run(event_opcode_e opcode) {
    if (opcode == EVENT_OPCODE_COMPARE_LESS_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            <= g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_OPCODE_COMPARE_GREATER_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            >= g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_OPCODE_COMPARE_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            == g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_OPCODE_COMPARE_NOT_EQUAL) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            != g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_OPCODE_COMPARE_LESS) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            < g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
    if (opcode == EVENT_OPCODE_COMPARE_GREATER) {
        g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT] = g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_RESULT]
            > g_script_variables[EVENT_SCRIPT_VAR_COMPARISON_OPERAND];
        return;
    }
}

/* Run one variable command on a script variable of any range (32-bit value, flag bit or
 * packed nibble): zero it, or add, subtract, multiply, divide, take the modulo, AND or OR
 * an operand into it. Odd opcodes read the operand from variable `source`, even ones take
 * `source` as an immediate. Flags and nibbles are read and written in place, keeping
 * their neighbors.
 *
 * EVENT_SCRIPT_VAR_RANDOM_VALUE is read through the event
 * RNG, not its ordinary storage. ZERO never reads the uninitialized operand.
 * Keep the target's arithmetic edge behavior: only division explicitly stops
 * on zero; modulo has no such check, and signed overflow is not normalized. */
void script_variable_command_run(s32 opcode, s32 destination_id, s32 source, s32 unused) {
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
            operand = random16_next();
        } else {
            source_word = variable_word_get(source);
            source_bit = variable_shift_get(source);
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
    destination_word = variable_word_get(destination_id);
    destination_bit = variable_shift_get(destination_id);
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
            thread_current_exit();
        }
        value = value / operand;
    } else if (opcode < EVENT_OPCODE_AND_IMMEDIATE) {
        value = value % operand;
    } else if (opcode < EVENT_OPCODE_OR_IMMEDIATE) {
        value &= operand;
    } else if (opcode < EVENT_OPCODE_ZERO_VARIABLE) {
        value |= operand;
    }
    /* Two vetoes, skipped while the guard is set so the getters below (which run this
     * function themselves) can read: a variable from EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST
     * up is written only while its value is 0-11, and while progress effects are suppressed
     * (replay mode) only the variables the conditions below list may change. */
    if (g_script_variable_write_guard == 0) {
        if (destination_id >= EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST) {
            g_script_variable_write_guard = 1;
            write_allowed = (u32)script_variable_get(destination_id) < 12;
            g_script_variable_write_guard = 0;
            if (!write_allowed) {
                return;
            }
        }
        if (g_script_variable_write_guard == 0) {
            g_script_variable_write_guard = 1;
            replay_mode = script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS);
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
static s32* variable_word_get(s32 variable_id) {
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
        thread_current_exit();
    }
    return word;
}

/* Return the packed variable field position, or -1 for a full word; invalid ids exit the thread. */
static s32 variable_shift_get(s32 value) {
    s32 result = -1;

    if (value >= EVENT_SCRIPT_VAR_PACKED_BIT_FIRST) {
        if (value < EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST) {
            result = value & 0x1f;
        } else if (value < EVENT_SCRIPT_VAR_END) {
            result = (value & 7) * 4;
        } else {
            thread_current_exit();
        }
    }
    return result;
}

/* Wait until the variable reaches the signed 16-bit test value. */
void script_opcode_7e_wait_value(const event_wait_value_parameters_t* parameters) {
    s32 variable_id = script_s16_read(parameters->variable_id_le);
    s32 test_value = script_s16_read(parameters->test_value_le);

    while (script_variable_get(variable_id) < test_value) {
        thread_yield();
    }
}

/* Seed the event script's 16-bit random generator (random16_next) with
 * the vertical-blank count. */
void script_random_seed(void) {
    g_script_rand16_state = VSync(-1);
}

/* Step the event script's random generator, state * 0x6255 + 0x3619 kept to 16
 * bits, and return the new state. script_variable_command_run calls it
 * when a variable opcode reads EVENT_SCRIPT_VAR_RANDOM_VALUE. */
static s32 random16_next(void) {
    return g_script_rand16_state = (g_script_rand16_state * 0x6255U + 0x3619U) & 0xffffU;
}

/* Yield while the unit-ready test returns zero. */
void script_unit_ready_wait(s32 runtime_id) {
    while (unit_is_ready(runtime_id) == 0) {
        thread_yield();
    }
}

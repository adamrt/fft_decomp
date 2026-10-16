/* Background sound threads, thread slot and task lookups, jump target searches, script conditions and variables, the
 * event random generator, text section pointers, event input, primitive and draw mode helpers and number glyphs. */

#include "fft/battle.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static s32 script_get_rand16(void);
static s32 script_get_variable_bit_position(s32 value);
static s32* script_get_variable_word_pointer_from_id(s32 variable_id);

/* Thread task 0x35, started by the event script's background-sound instruction:
 * restart an environment sound (sound resource ENV), on a free SFX channel pair
 * when find_channel is set and as the remembered background sound otherwise, and
 * set its volume (0 becomes 1).
 *
 * It then continues in sound_run_background_fade_thread, which reads the same
 * parameter record to fade from volume_from to volume_to over frames yields. */
void sound_run_background_sfx_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 sound;
    s32 volume;
    s32 find_channel;
    s32 resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1();
    find_channel = request->find_channel;
    sound_id = request->sound_id;
    if (find_channel != 0) {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_sfx_find_channel(sound);
    } else {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_bg_sfx(sound);
    }
    volume = request->volume_from;
    if (volume == 0) {
        volume = 1;
    }
    main_sound_set_sfx_echo(sound_id | resource, volume);
    sound_run_background_fade_thread();
}

/* Thread task 0x35: ramp the volume of an environment sound from volume_from to
 * volume_to over `steps` yields, then settle on the final level and exit the
 * thread.
 *
 * A step's volume is never 0, which would stop the sound: a negative level is
 * negated and 0 becomes 1. The event script's edit-background-sound instruction
 * starts it as a thread, and sound_run_background_sfx_thread continues in it. */
void sound_run_background_fade_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 volume_from;
    s32 volume_to;
    s32 steps;
    s32 delta;
    s32 scaled;
    s32 volume;
    s32 step;
    s32 resource;
    /* The target reserves eight frame bytes that no other local
     * accounts for; the pad reproduces the 0x40-byte frame. */
    s32 stack_pad[2];

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    step = 0;
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1();
    sound_id = request->sound_id;
    volume_from = request->volume_from;
    steps = request->frames;
    volume_to = request->volume_to;
    resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);
    if (steps != 0) {
        delta = volume_to - volume_from;
        scaled = 0;
        do {
            thread_yield();
            volume = (scaled / steps) + volume_from;
            if (volume < 0) {
                volume = -volume;
            }
            if (volume == 0) {
                volume = 1;
            }
            scaled += delta;
            main_sound_set_sfx_echo(sound_id + resource, volume);
            step++;
        } while (step < steps);
    }
    main_sound_set_sfx_echo(sound_id | resource, volume_to);
    thread_exit_current();
}

/* Event instruction 0x37 (unused by the scenario scripts): toggles bit 0 of
 * portrait_flipped of the menu window buffer whose message_id matches the
 * operand.
 *
 * record_index is never assigned: the target indexes with whatever $s0 holds
 * on entry and tests that one record six times. */
void script_toggle_message_portrait_flip(const u8* parameters) {
    s32 i;
    s32 record_index;
    s16 record_id = script_read_s16(parameters);
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
void script_opcode_unknown_74(const u8* unused_parameters) {
}

/* Returns the slot a thread should start in: a requested id below 16 as it is,
 * otherwise (THREAD_SLOT_FIRST_IDLE) the first idle slot from 1 up. With
 * every slot busy the calling thread exits, and the function has no return value
 * for that path. */
s32 thread_resolve_id(s32 requested_thread_id) {
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
    thread_exit_current();
}

/* As thread_resolve_id, but the search for an idle slot starts above the
 * current thread's slot. */
s32 thread_resolve_id_after_current(s32 thread_id) {
    s32 index;

    if (thread_id < THREAD_ID_COUNT) {
        return thread_id;
    }

    index = g_thread_current_id + 1;
    if (index >= VARIANT_THREAD_TABLE_SLOT_COUNT) {
        thread_exit_current();
    } else {
        do {
            if (thread_is_running_asm(index) == 0) {
                return index;
            }
            index++;
        } while (index < VARIANT_THREAD_TABLE_SLOT_COUNT);

        thread_exit_current();
    }
}

/* Return the id of another running thread whose 32-bit current task value equals
 * task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < VARIANT_THREAD_TABLE_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}

/* Tag the running thread with task_id, the value thread_find_running_by_task looks up. */
void thread_set_current_task_id(s32 task_id) {
    g_thread_contexts[g_thread_current_id].task_id = task_id;
}

/* Find a jump's target and return the offset after its two bytes. For BackTarget it scans from the
 * event start to limit_offset and keeps the last matching BackTarget not followed by a barrier
 * (0xd9) or a same-id ForwardTarget; otherwise it scans forward from limit_offset to EventEnd for
 * target_opcode or alternate_opcode. With no target the thread exits. */
s32 script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id) {
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
    /* Two vetoes, skipped while the guard is set so the getters below (which run this
     * function themselves) can read: a variable from EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST
     * up is written only while its value is 0-11, and while progress effects are suppressed
     * (replay mode) only the variables the conditions below list may change. */
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
static s32* script_get_variable_word_pointer_from_id(s32 variable_id) {
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
static s32 script_get_variable_bit_position(s32 value) {
    s32 result = -1;

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
void script_opcode_wait_value(const event_wait_value_parameters_t* parameters) {
    s32 variable_id = script_read_s16(parameters->variable_id_le);
    s32 test_value = script_read_s16(parameters->test_value_le);

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
static s32 script_get_rand16(void) {
    return g_script_rand16_state = (g_script_rand16_state * 0x6255U + 0x3619U) & 0xffffU;
}

/* Yield while the unit-ready test returns zero. */
void script_wait_for_unit_ready(s32 runtime_id) {
    while (battle_return_one(runtime_id) == 0) {
        thread_yield();
    }
}

/* BATTLE leaves the last section alone. */
#ifdef FFT_BATTLE
#    define TEXT_RELOCATED_SECTION_COUNT (TEXT_SECTION_COUNT - 1)
#else
#    define TEXT_RELOCATED_SECTION_COUNT TEXT_SECTION_COUNT
#endif

/* Turn the text file's table of section offsets into g_text_section_pointers.
 * The text begins after the table of TEXT_SECTION_COUNT offsets, which are
 * relative to its end. BATTLE leaves the last section alone and, while menu
 * input is disabled, keeps section 0 bound to the event or tutorial text
 * already in use. */
void text_init_section_pointers(void) {
    s32* source;
    u8** destination;
    /* These two assignments fill the target load delay before the loop. */
    s32* text;
#ifdef FFT_BATTLE
    s32 menu_input_mode;
#endif
    s32 index;
    s32 offset;

    source = g_text_section_offsets;
    index = 0;
#ifdef FFT_BATTLE
    menu_input_mode = g_menu_input_mode;
#endif
    text = source + TEXT_SECTION_COUNT;
    destination = g_text_section_pointers;
    do {
#ifdef FFT_BATTLE
        if (menu_input_mode == MENU_INPUT_PLAYER || index != TEXT_SECTION_EVENT_MESSAGES)
#endif
        {
            offset = *source;
            offset += (s32)text;
            *destination = (u8*)offset;
        }
        source++;
        index++;
        destination++;
    } while (index < TEXT_RELOCATED_SECTION_COUNT);
}

/* Return a pointer to the event input's newly pressed buttons, or to its held buttons when
 * use_current is nonzero. */
u32* script_get_controller_input_pointer(s32 use_current) {
    u32* result = &g_script_event_input;
    if (use_current != 0) {
        result = &g_script_event_current_input;
    }
    return result;
}

/* Copy a saved input state of six 32-bit values (pressed, held and previous buttons and the
 * three repeat counters) into the event input globals. The BUNIT and CARD
 * overlays call it after computing the state themselves. */
void script_load_event_input_state(const menu_input_state_t* saved) {
    g_script_event_input = saved->new_buttons;
    g_script_event_current_input = saved->current_buttons;
    g_script_event_previous_input = saved->previous_buttons;
    g_script_event_initial_repeat_counter = saved->initial_repeat_counter;
    g_script_event_repeat_counter = saved->repeat_counter;
    g_script_event_secondary_repeat_counter = saved->secondary_repeat_counter;
}

/* Draw a GPU primitive at once with DrawPrim when immediate drawing is on, otherwise link it into
 * the current ordering-table entry. */
void gfx_draw_or_append_gpu_primitive(void* primitive) {
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawPrim(primitive);
        return;
    }
    addPrim(g_gfx_current_otag_entry, primitive);
}

/* Link a GPU primitive into the ordering-table entry after the current one (libgpu addPrim). */
void gfx_append_gpu_primitive_to_secondary_otag(void* prim) {
    u32* entries = g_gfx_current_otag_entry;

    addPrim(&entries[1], prim);
}

/* Store the rectangle text variables $7E and $7F substitute. */
void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height) {
    g_text_substitution_rect.x = x;
    g_text_substitution_rect.y = y;
    g_text_substitution_rect.w = width;
    g_text_substitution_rect.h = height;
}

/* Set the screen origin used by menu text glyphs. */
void menu_set_text_origin(s32 x, s32 y) {
    g_menu_text_state.pen.rect.x = x;
    g_menu_text_state.pen.rect.y = y;
}

/* Set a draw mode whose texture page is the VRAM area of the given menu texture page
 * (menu_texture_page_e); any page above 3 selects the item icon page at (896, 288). */
void gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page) {
    if (page == MENU_TEXTURE_PAGE_FRAME) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_ALLOCATED_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x1c0, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_TEXT_IMAGES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x180, 0), &g_gfx_texture_window);
    } else if (page == MENU_TEXTURE_PAGE_UNIT_SPRITES) {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x340, 0x100), &g_gfx_texture_window);
    } else {
        SetDrawMode(mode, 0, 0, GetTPage(0, 0, 0x380, 0x120), &g_gfx_texture_window);
    }
}

/* Set a draw mode from aligned texture-page coordinates from the menu text image origin. */
void gfx_set_draw_mode_from_origin(DR_MODE* mode, const point16_t* origin) {
    SetDrawMode(mode, 0, 0, GetTPage(0, 0, origin->x & ~0x3f, origin->y & ~0xff), &g_gfx_texture_window);
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * gfx_set_draw_mode_from_origin and script_noop_10. MAIN calls BATTLE's copy
 * through main_overlay_call_battle_return_zero while script variable 0xff is
 * set. Nothing else references it; the name is a neutral placeholder until a
 * role turns up. */
s32 script_return_zero_1(void) {
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_return_zero_1
 * and script_frame_after_requests_stub. Nothing on the disc calls it or points
 * at it; the name is a neutral placeholder until a reference turns up. */
void script_noop_10(void) {
}

/* Empty. The event frame runner calls it each frame after the pending requests. */
void script_frame_after_requests_stub(void) {
}

/* Point a sprite (SPRT) or textured quad (POLY_FT4) at an image: texture source from parameters
 * relative to base_load, screen rectangle relative to base_screen. A negative x_load keeps the
 * primitive's texture fields and moves it only. */
void gfx_init_image_loading(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_load_parameters_t* parameters) {
    int texture_column;
    SPRT* sprite = primitive;
    POLY_FT4* quad = primitive;

    /* GPU codes 0x64-0x67 are the free-size sprite (SPRT). */
    if ((getcode(primitive) & 0x7c) == 0x64) {
        if (parameters->x_load >= 0) {
            /* A sprite uses the current texture page: u is the 4bpp pixel column within it. */
            texture_column = base_load->x & 0x3f;
            sprite->u0 = (texture_column << 2) + parameters->x_load;
            sprite->v0 = base_load->y + parameters->y_load;
            sprite->w = parameters->width;
            sprite->h = parameters->height;
        }
        sprite->x0 = base_screen->x + parameters->x_screen_offset;
        sprite->y0 = base_screen->y + parameters->y_screen_offset;
        return;
    }

    if (parameters->x_load >= 0) {
        quad->tpage = GetTPage(0, 0, base_load->x, base_load->y & 0xf00);
        setUV4(quad, parameters->x_load, base_load->y + parameters->y_load, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load, parameters->x_load,
            base_load->y + parameters->y_load + parameters->height, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load + parameters->height);
    }

    setXY4(quad, base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset + parameters->width, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset + parameters->height,
        base_screen->x + parameters->x_screen_offset + parameters->width,
        base_screen->y + parameters->y_screen_offset + parameters->height);
}

/* Draw count positioned number entries into an image buffer: kind 0 with
 * text_draw_large_number_glyphs, kind 1 with menu_draw_number_glyphs; other kinds draw
 * nothing. */
void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->kind == 0) {
            text_draw_large_number_glyphs(*entries->value, entries->format, buffer, position);
        } else if (entries->kind == 1) {
            menu_draw_number_glyphs(*entries->value, entries->format, buffer, position);
        }
        entries++;
    }
}

/* Draw value in decimal text glyphs, right-aligned in a field of digit_count 6-pixel cells at
 * origin, without zero padding (a longer number runs on to the left). Moves origin and
 * clears the text palette offset afterward. */
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin) {
    s16 cell;

    origin->rect.x += (digit_count - 1) * 6;
    do {
        text_render_glyph_to_4bpp_image(
            g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, origin, g_menu_text_palette_offset);
        value = value / 10;
        cell = origin->rect.x;
        origin->rect.x = cell - 6;
    } while (value != 0);
    g_menu_text_palette_offset = 0;
}

/* Draw value as exactly (digits & 0xff) digits, zero-padded and right to left, 7 pixels apart from
 * the menu glyph sheet. The high byte adds marks: 0x1000 a leading glyph, MENU_DECIMAL_SIGN_PLUS
 * (0x400) or MENU_DECIMAL_SIGN_MINUS (0x800) a sign glyph before the digits, and 0xc00 two
 * dashes in place of the number. Hidden numeric values draw
 * a placeholder per digit; the pen ends after the last digit. */
void menu_draw_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src;

    /* One variable holds the sign-glyph selector first and the x to leave the
     * cursor at afterwards; the target keeps both roles in $s6. */
    src = &g_menu_number_glyph;
    g_menu_number_glyph.rect.h = 0xF;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00) {
        pos->rect.x += 2;
        src->rect.x = 0xBA;
        g_menu_number_glyph.rect.y = 0xD;
        g_menu_number_glyph.rect.w = 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        g_menu_number_glyph.rect.w = 8;
        return;
    }
    if (digits & 0x1000) {
        src->rect.x = 0xC8;
        g_menu_number_glyph.rect.y = 0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if ((digits & 0x800) || (digits & 0x400)) {
        if (digits & 0x400) {
            src->rect.x = 0xC8;
        } else {
            src->rect.x = 0xBA;
        }
        src->rect.y = 0xD;
        src->rect.w = 6;
        pos->rect.x -= 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        pos->rect.x += 6;
        src->rect.w = 8;
    }
    /* Not a variable: the target recomputes the digit count at the loop test
     * rather than keeping it live, so each use spells the mask out. */
    pos->rect.x += ((digits & 0xFF) - 1) * 7;
    cursor = pos->rect.x + 7;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.y = 0;
        src->rect.x = (value % 10) * 8 + 0x78;
        if (g_menu_hide_numeric_values != 0) {
            src->rect.x = 0x48;
            src->rect.y = 0x10;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        /* After the call: the quotient stays live past the argument setup, so
         * it takes a callee-saved register and the update lands in the delay
         * slot, as the target does at 0x238. */
        value /= 10;
        pos->rect.x -= 7;
    }
    pos->rect.x = cursor;
}

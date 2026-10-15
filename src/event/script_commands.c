/* Script sound camera and menus, one object of the library BATTLE and WORLD link in the same order; cuts inside the
 * library are chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

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
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
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
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
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
    s16 record_id;
    menu_window_buffer_t* record;

    record_id = script_read_s16(parameters);
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
void script_opcode_74_event_instruction(const u8* unused_parameters) {
}

/* Returns the slot a thread should start in: a requested id below 16 as it is,
 * otherwise (THREAD_SLOT_FIRST_IDLE) the first idle slot from 1 up. With
 * every slot busy the calling thread exits, and the function has no return value
 * for that path. */
s32 thread_resolve_id(s32 requested_thread_id) {
    s32 i;

    if (requested_thread_id < THREAD_RESET_SLOT_COUNT) {
        return requested_thread_id;
    }
    i = 1;
    do {
        if (thread_is_running_asm_variant(i) == 0) {
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

    if (thread_id < THREAD_RESET_SLOT_COUNT) {
        return thread_id;
    }

    index = g_thread_current_id + 1;
    if (index >= VARIANT_THREAD_TABLE_SLOT_COUNT) {
        thread_exit_current();
    } else {
        do {
            if (thread_is_running_asm_variant(index) == 0) {
                return index;
            }
            index++;
        } while (index < VARIANT_THREAD_TABLE_SLOT_COUNT);

        thread_exit_current();
    }
}

/* Return the id of another running thread whose 32-bit current task value equals
   task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < VARIANT_THREAD_TABLE_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm_variant(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}

void thread_set_current_task_id(s32 task_id) {
    g_thread_contexts[g_thread_current_id].task_id = task_id;
}

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

#ifdef FFT_BATTLE
/* Relocate the 31 embedded menu-text offsets into the active section table.
 *
 * The text begins after a header of 32 32-bit offsets. When menu input is disabled,
 * section 0 remains bound to the event/tutorial text already in use. */
void text_init_section_pointers_variant(void) {
    s32* source;
    u8** destination;
    /* These two assignments fill the target load delay before the loop. */
    s32* text;
    s32 menu_input_mode;
    s32 index;
    s32 offset;

    source = g_battle_menu_text_section_offsets;
    index = 0;
    menu_input_mode = g_menu_input_mode;
    text = source + 32;
    destination = g_text_section_pointers;
    do {
        if (menu_input_mode == MENU_INPUT_PLAYER || index != TEXT_SECTION_EVENT_MESSAGES) {
            offset = *source;
            offset += (s32)text;
            *destination = (u8*)offset;
        }
        source++;
        index++;
        destination++;
    } while (index < 31);
}
#endif

#ifdef FFT_WORLD
/* Turn the text file's table of section offsets into
 * g_text_section_pointers. The offsets are relative to the end of the
 * table. */
void text_init_section_pointers_variant(void) {
    s32* offset;
    u8** section;
    s32 i;
    u8* text;
    s32 value;

    offset = g_text_section_offsets;
    i = 0;
    text = (u8*)(offset + TEXT_SECTION_COUNT);
    section = g_text_section_pointers;
    for (; i < TEXT_SECTION_COUNT; i++) {
        value = *offset;
        offset++;
        value += (s32)text;
        *section = (u8*)value;
        section++;
    }
}
#endif

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

void gfx_draw_or_append_gpu_primitive(void* primitive) {
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawPrim(primitive);
        return;
    }
    addPrim(g_gfx_current_otag_entry, primitive);
}

/* Link a GPU primitive into the current ordering table entry (libgpu addPrim). */
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

void gfx_init_image_loading(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_load_parameters_t* parameters) {
    int texture_column;
    SPRT* sprite = primitive;
    POLY_FT4* quad = primitive;

    if ((getcode(primitive) & 0x7c) == 0x64) {
        if (parameters->x_load >= 0) {
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

void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->kind == 0) {
            text_draw_large_number_glyphs_variant(*entries->value, entries->format, buffer, position);
        } else if (entries->kind == 1) {
            menu_draw_number_glyphs(*entries->value, entries->format, buffer, position);
        }
        entries++;
    }
}

/* Draw decimal glyphs right to left and clear the text palette afterward. */
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

#ifdef FFT_BATTLE
/* Draws `value` right to left as `digits & 0xff` large glyphs at `pos`, with
 * the sign and marker glyphs selected by the flag bits of `digits`. Sibling of
 * menu_draw_number_glyphs (0x8014ac30) on the 5-pixel font. WORLD links its
 * own body under this name: it lacks the leading 0x800 glyph and the hidden
 * value check, and draws every digit as 0xba under flag 0x800 instead. */
void text_draw_large_number_glyphs_variant(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src;

    src = &g_text_large_number_glyph_rect;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00 || cursor == 0xE00) {
        g_text_large_number_glyph_rect.rect.y = 0x10;
        if (digits & 1) {
            src->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
            pos->rect.x += 5;
        }
        src->rect.x = 0xBA;
        if (cursor == 0xE00) {
            src->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
            pos->rect.x += 5;
        }
        return;
    }
    if (digits & 0x800) {
        src->rect.x = 0xBA;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if (g_menu_hide_numeric_values == 0) {
        if ((digits & 0x200) && value == 0x64) {
            digits = cursor + 3;
        }
        if ((digits & 0x400) && value != 0x64) {
            digits = (digits & 0xFF00) + 2;
            pos->rect.x += 6;
        }
    }
    if (digits & 0x1000) {
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
    if (digits & 0x8000) {
        pos->rect.x -= 2;
        src->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 7;
    }
    if (digits & 0x100) {
        src->rect.x = 0x78;
        src->rect.y = 0x1A;
        src->rect.h = 4;
        pos->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 7;
        pos->rect.y -= 3;
        src->rect.y = 0x10;
        src->rect.h = 0xA;
    }
    pos->rect.x += ((digits & 0xFF) - 1) * 5;
    cursor = pos->rect.x + 5;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.x = (value % 10) * 6 + 0x78;
        if (g_menu_hide_numeric_values != 0) {
            src->rect.x = 0xD0;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        value /= 10;
        pos->rect.x -= 5;
    }
    pos->rect.x = cursor;
    if (digits & 0x2000) {
        src->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, pos);
        pos->rect.x += 5;
    }
}
#endif

#ifdef FFT_WORLD
/*
 * Render a decimal value into a text image, right to left, as `flags & 0xff`
 * digit glyphs with optional sign/prefix glyphs selected by the high flag bits.
 *
 * Prefix modes 0xc00 and 0xe00 instead draw three placeholder glyphs (0xba or
 * 0xd0), optionally preceded by the 0xb4 glyph. The cursor ends five pixels
 * past the last digit. `work` holds the prefix mode and later the final cursor
 * x; sharing one variable reproduces the target's register assignment.
 * BATTLE links its own, longer body under this name.
 */
void text_draw_large_number_glyphs_variant(s32 value, s32 flags, u8* pixels, glyph_blit_t* position) {
    glyph_blit_t* glyph;
    s32 work;
    s32 i;

    work = flags & 0xFF00;
    glyph = &g_text_large_number_glyph_rect;
    if (work == 0xC00 || work == 0xE00) {
        g_text_large_number_glyph_rect.rect.y = 0x10;
        if (flags & 1) {
            glyph->rect.x = 0xB4;
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, glyph, position);
            position->rect.x += 5;
        }
        glyph->rect.x = 0xBA;
        if (work == 0xE00) {
            glyph->rect.x = 0xD0;
        }
        for (i = 0; i < 3; i++) {
            text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
            position->rect.x += 5;
        }
        return;
    }
    if (g_menu_hide_numeric_values == 0) {
        if ((flags & 0x200) && value == 100) {
            flags = work + 3;
        }
        if ((flags & 0x400) && value != 100) {
            flags = (flags & 0xFF00) + 2;
            position->rect.x += 6;
        }
    }
    if (flags & 0x1000) {
        glyph->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
    if (flags & 0x8000) {
        position->rect.x -= 2;
        glyph->rect.x = 0xB4;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 7;
    }
    if (flags & 0x100) {
        glyph->rect.x = 0x78;
        glyph->rect.y = 0x1A;
        glyph->rect.h = 4;
        position->rect.y += 3;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 7;
        position->rect.y -= 3;
        glyph->rect.y = 0x10;
        glyph->rect.h = 0xA;
    }
    position->rect.x += ((flags & 0xFF) - 1) * 5;
    work = position->rect.x + 5;
    for (i = 0; i < (flags & 0xFF); i++) {
        glyph->rect.x = value % 10 * 6 + 0x78;
        if (flags & 0x800) {
            glyph->rect.x = 0xBA;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        value /= 10;
        position->rect.x -= 5;
    }
    position->rect.x = work;
    if (flags & 0x2000) {
        glyph->rect.x = 0xC0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_text_large_number_glyph_rect, position);
        position->rect.x += 5;
    }
}
#endif

void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position) {
    position->rect.x -= 2;
    position->rect.y += 2;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_upper, position);
    position->rect.y += 4;
    text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_stacked_glyph_lower, position);
}

/* Dispatch one text entry through the scene's main-stack renderer. */
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 0);
}

/* Dispatch with the renderer's continue-past-choice-marks argument. */
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(text_id, image, origin, 1);
}

/* Dispatch a supplied text cursor through the scene's main-stack renderer. */
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text) {
    g_thread_call_target = (void (*)(void))menu_display_text_variant;
    thread_call_on_main_stack(0, image, origin, text);
}

#ifdef FFT_BATTLE
/* Renders a text entry's glyphs into a menu image at the pen position.
 *
 * The thread target installed by menu_display_text_entry. text 0 or 1
 * selects text_id's entry.
 * Handles the substitution, script-variable, palette, width and numeric
 * control codes; glyph drawing is limited to the characters counted from
 * g_battle_menu_text_param_22 up to g_battle_menu_text_param_24, which are reset on exit.
 *
 * Reading each control operand through the byte local and the u8
 * variable_base keep the outer loop large enough that loop.c does not hoist
 * the 0xFFFF pending-width constant, as in the target. */
void menu_display_text_variant(s32 text_id, u8* image, glyph_blit_t* origin, u8* text) {
    text_reader_state_t* state;
    u8* cursor;
    u8 operand;
    s32 width;
    u32* divisor;
    u32* powers;
    glyph_blit_t* pen;
    s32 digit;
    s32 started;
    s32 count;
    u8 variable_base;

    main_party_get_unit(0);
    if (text == 0 || text == (u8*)1) {
        cursor = text_find_entry(text_id);
    } else {
        cursor = text;
    }
    /* The renderer uses the scratchpad block after the text measurer. */
    state = (text_reader_state_t*)getScratchAddr(128);
    powers = g_text_decimal_divisors;
    variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    state->return_cursor = (u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->pending_width = 0xFFFF;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;
    g_menu_text_pen.rect.x = origin->rect.x;
    pen = &g_menu_text_pen;
    count = 0;
    g_text_glyph_first_row = 0;
    g_menu_text_pen.rect.y = origin->rect.y;
    g_text_glyph_row_limit = 0x10;
    g_menu_text_pen.stride = origin->stride;

    while (1) {
        state->command = *cursor;
        if (state->command >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            if ((state->command & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                state->command = (*cursor & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                state->command = *cursor + state->command;
            } else {
                if (state->command == TEXT_SET_GLYPH_DELAY) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                } else if (state->command == TEXT_SET_PORTRAIT) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, operand);
                } else if (state->command == TEXT_SET_VARIABLE_BASE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    variable_base = operand;
                } else if (state->command == TEXT_STORE_VARIABLE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    digit = script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
                    if (digit != 0) {
                        script_set_variable(variable_base + digit * 10, *cursor);
                        script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
                    } else {
                        script_set_variable(variable_base, *cursor);
                    }
                } else if (state->command == TEXT_SET_PALETTE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    g_menu_text_palette_offset = operand;
                } else if (state->command == TEXT_CHOICE_MARK) {
                } else if (state->command == TEXT_SPACE) {
                    pen->rect.x += 4;
                } else if (state->command == TEXT_NEWLINE) {
                    pen->rect.x = origin->rect.x;
                    if (count >= g_menu_text_param_22) {
                        g_text_glyph_first_row = 0;
                        pen->rect.y += 16;
                    }
                    count++;
                } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    state->pending_width = operand;
                } else if (state->command == TEXT_PRINT_NEXT_VALUE || state->command == TEXT_PRINT_INDEXED_VALUE) {
                    if (state->command == TEXT_PRINT_NEXT_VALUE) {
                        state->value = *state->value_cursor;
                        state->value_cursor++;
                    } else {
                        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                        operand = *cursor;
                        state->value = g_text_substitution_values[operand];
                    }
                    width = state->pending_width;
                    if (width == 0xFFFF) {
                        started = 0;
                        digit = text_count_decimal_digits(state->value) - 1;
                    } else {
                        state->pending_width = 0xFFFF;
                        digit = width - 1;
                        started = 0;
                    }
                    if (digit >= 0) {
                        divisor = (u32*)(digit * 4 + (s32)powers);
                        do {
                            if (state->value >= *divisor || digit == 0 || started != 0) {
                                state->command = state->value / *divisor;
                                started = 1;
                                state->value = state->value - state->command * *divisor;
                                if (count >= g_menu_text_param_24) {
                                    goto done;
                                }
                                if (count >= g_menu_text_param_22) {
                                    text_render_glyph_to_4bpp_image(
                                        g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                                        &g_menu_text_pen, g_menu_text_palette_offset);
                                }
                            }
                            divisor--;
                            digit--;
                            pen->rect.x += 6;
                        } while (digit >= 0);
                    }
                }
                state->value = text_resolve_format_string_id(state->command, *state->value_cursor);
                if (state->value != -1) {
                    state->return_cursor = cursor + 1;
                    if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                        cursor = g_text_player_name_buffer;
                    } else {
                        cursor = text_find_entry(state->value);
                    }
                    state->value_cursor++;
                    continue;
                }
                if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
                    state->return_cursor = cursor + 3;
                    state->scratch = (cursor[0] & 3) * 8;
                    state->remaining_bytes = cursor[1] >> 5;
                    state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
                    state->scratch = (cursor[1] & 0xF) * 0xFE;
                    state->scratch = cursor[2] + state->scratch;
                    cursor -= state->scratch;
                    continue;
                }
                if ((state->command & 0xFE) == 0xFE) {
                    if (state->return_cursor == (u8*)-1) {
                        break;
                    }
                    cursor = state->return_cursor;
                    state->return_cursor = (u8*)-1;
                    continue;
                }
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                continue;
            }
        }
        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
        if (count >= g_menu_text_param_24) {
            break;
        }
        if (count >= g_menu_text_param_22) {
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                &g_menu_text_pen, g_menu_text_palette_offset);
        }
        pen->rect.x += g_text_glyph_widths[state->command];
        if (pen->rect.x >= pen->stride - 8) {
            break;
        }
    }
done:
    g_menu_text_palette_offset = 0;
    g_menu_text_param_22 = 0;
    g_menu_text_param_24 = 0xFF;
}
#endif

#ifdef FFT_WORLD
/* Render a WORLD text entry glyph by glyph into a menu window image.
 *
 * `text` of 0 or 1 selects `text_id` from the text tables; 1 also lets the
 * 0xFB command continue instead of ending the text. Walks the same command
 * stream as text_measure, drawing lines from g_menu_text_param_22
 * up to g_menu_text_param_24 and stopping at the window's right margin.
 * Numbers (0xE4/0xE6) are drawn one digit per 6-pixel column. Indexing the
 * divisor table inside the digit loop, rather than walking a pointer, is
 * what gives the target's strength-reduced pointer and register choice. */
void menu_display_text_variant(s32 text_id, u8* pixels, glyph_blit_t* origin, u8* text) {
    party_unit_t* party;
    u8* cursor;
    s32 i;
    u8* fill = g_text_player_name_buffer;
    u8 terminator;
    text_reader_state_t* state;
    s32 variable_id;
    s32 line;
    s32 started;

    party = main_party_get_unit(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < 16; i++) {
        g_text_player_name_buffer[i] = party->name[i];
    }
    if (text == 0 || text == (u8*)1) {
        cursor = text_find_entry(text_id);
    } else {
        cursor = text;
    }
    state = (text_reader_state_t*)getScratchAddr(128);
    variable_id = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    state->return_cursor = (u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->pending_width = 0xFFFF;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;
    g_menu_text_pen.rect.x = origin->rect.x;
    g_text_glyph_first_row = 0;
    g_menu_text_pen.rect.y = origin->rect.y;
    g_text_glyph_row_limit = 0x10;
    g_menu_text_pen.stride = origin->stride;
    line = 0;

    while (1) {
        state->command = *cursor;
        if (state->command >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            if ((state->command & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                state->command = (*cursor & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                state->command = *cursor + state->command;
            } else {
                if (state->command == TEXT_SET_GLYPH_DELAY) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                } else if (state->command == TEXT_SET_PORTRAIT) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *cursor);
                } else if (state->command == TEXT_SET_VARIABLE_BASE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    variable_id = *cursor;
                } else if (state->command == TEXT_STORE_VARIABLE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    i = script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
                    if (i != 0) {
                        script_set_variable(variable_id + i * 10, *cursor);
                        script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
                    } else {
                        script_set_variable(variable_id, *cursor);
                    }
                } else if (state->command == TEXT_SET_PALETTE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    g_menu_text_palette_offset = *cursor;
                } else if (state->command == TEXT_CHOICE_MARK) {
                    if (text != (u8*)1) {
                        break;
                    }
                } else if (state->command == TEXT_SPACE) {
                    g_menu_text_pen.rect.x += 4;
                } else if (state->command == TEXT_NEWLINE) {
                    g_menu_text_pen.rect.x = origin->rect.x;
                    if (line >= g_menu_text_param_22) {
                        g_text_glyph_first_row = 0;
                        g_menu_text_pen.rect.y += 0x10;
                    }
                    line++;
                } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    state->pending_width = *cursor;
                } else if (state->command == TEXT_PRINT_NEXT_VALUE || state->command == TEXT_PRINT_INDEXED_VALUE) {
                    if (state->command == TEXT_PRINT_NEXT_VALUE) {
                        state->value = *state->value_cursor;
                        state->value_cursor++;
                    } else {
                        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                        state->value = g_text_substitution_values[*cursor];
                    }
                    i = state->pending_width - 1;
                    if (state->pending_width == 0xFFFF) {
                        started = 0;
                        i = text_count_decimal_digits(state->value) - 1;
                    } else {
                        state->pending_width = 0xFFFF;
                        started = 0;
                    }
                    for (; i >= 0; i--) {
                        if (state->value >= g_text_decimal_divisors[i] || i == 0 || started != 0) {
                            state->command = state->value / g_text_decimal_divisors[i];
                            started = 1;
                            state->value = state->value - state->command * g_text_decimal_divisors[i];
                            if (line >= g_menu_text_param_24) {
                                /* Leaves the digit loop and the text loop. */
                                goto end;
                            }
                            if (line >= g_menu_text_param_22) {
                                text_render_glyph_to_4bpp_image(
                                    g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, pixels,
                                    &g_menu_text_pen, g_menu_text_palette_offset);
                            }
                        }
                        g_menu_text_pen.rect.x += 6;
                    }
                }

                state->value = text_resolve_format_string_id(state->command, *state->value_cursor);
                if (state->value != -1) {
                    state->return_cursor = cursor + 1;
                    if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                        cursor = g_text_player_name_buffer;
                    } else {
                        cursor = text_find_entry(state->value);
                    }
                    state->value_cursor++;
                    continue;
                }
                if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
                    state->return_cursor = cursor + 3;
                    state->scratch = (cursor[0] & 3) * 8;
                    state->remaining_bytes = cursor[1] >> 5;
                    state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
                    state->scratch = (cursor[1] & 0xF) * 0xFE;
                    state->scratch = cursor[2] + state->scratch;
                    cursor -= state->scratch;
                    continue;
                }
                if ((state->command & 0xFE) == 0xFE) {
                    if (state->return_cursor == (u8*)-1) {
                        break;
                    }
                    cursor = state->return_cursor;
                    state->return_cursor = (u8*)-1;
                    continue;
                }
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                continue;
            }
        }
        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
        if (line >= g_menu_text_param_24) {
            break;
        }
        if (line >= g_menu_text_param_22) {
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, pixels,
                &g_menu_text_pen, g_menu_text_palette_offset);
        }
        g_menu_text_pen.rect.x += g_text_glyph_widths[state->command];
        if (g_menu_text_pen.rect.x >= g_menu_text_pen.stride - 8) {
            break;
        }
    }
end:
    g_menu_text_palette_offset = 0;
    g_menu_text_param_22 = 0;
    g_menu_text_param_24 = 0xFF;
}
#endif

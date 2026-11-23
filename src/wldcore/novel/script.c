/* Sound novel saved state, script execution, pending sounds, the countdown timer and the sound novel opcodes. */

#include "fft/gfx/display_object.h"
#include "fft/gfx/gfx.h"
#include "fft/menu/menu_screen.h"
#include "fft/script/script_variable.h"
#include "fft/sys/bits.h"
#include "fft/sys/file.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/novel.h"
#include "fft/world/window.h"
#include "fft/world/world_map.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/pad.h"
#include "psx/types.h"

static void novel_opcode_00_end_script(void);
static void novel_opcode_01_end_section(void);
static void novel_opcode_02_nop(void);
static void novel_opcode_03_start_text_thread_and_push_history(void);
static void novel_opcode_04_wait_for_text_thread(void);
static void novel_opcode_05_load_background_set(void);
static void novel_opcode_06_branch_to_section(void);
static void novel_opcode_07_branch_if_dialog_option_equal(void);
static void novel_opcode_08_branch_if_dialog_option_not_equal(void);
static void novel_opcode_09_wait_frames(void);
static void novel_opcode_0a_load_picture(void);
static void novel_opcode_0b_hide_picture(void);
static void novel_opcode_0c_set_parent_windows_visible(void);
static void novel_opcode_0d_start_text_thread(void);
static void novel_opcode_0e_branch_to_random_section(void);
static void novel_opcode_0f_branch_if_section_read(void);
static void novel_opcode_10_branch_if_section_unread(void);
static void novel_opcode_11_branch_if_saved_flag_set(void);
static void novel_opcode_12_branch_if_saved_flag_clear(void);
static void novel_opcode_13_branch_if_local_flag_set(void);
static void novel_opcode_14_branch_if_local_flag_clear(void);
static void novel_opcode_15_set_saved_flag(void);
static void novel_opcode_16_clear_saved_flag(void);
static void novel_opcode_17_set_local_flag(void);
static void novel_opcode_18_clear_local_flag(void);
static void novel_opcode_19_set_counter(void);
static void novel_opcode_1a_add_counter_delta(void);
static void novel_opcode_1b_subtract_counter_delta(void);
static void novel_opcode_1c_show_value_display(void);
static void novel_opcode_1d_hide_value_display(void);
static void novel_opcode_1e_apply_counter_delta(void);
static void novel_opcode_1f_play_sound(void);
static void novel_opcode_20_load_music(void);
static void novel_opcode_21_set_sound_mode(void);
static void novel_opcode_22_branch_if_equal(void);
static void novel_opcode_23_branch_if_not_equal(void);
static void novel_opcode_24_branch_if_less(void);
static void novel_opcode_25_branch_if_greater(void);
static void novel_opcode_26_fade_out_music(void);
static void novel_opcode_27_switch_music_track(void);
static void novel_opcode_28_fade_screen_out(void);
static void novel_opcode_29_fade_screen_in(void);
static void novel_opcode_2a_set_white_screen_overlay(void);
static void novel_opcode_2b_wait_for_circle_press(void);
static void novel_opcode_2c_play_sound_effect_quad(void);
static void novel_opcode_2d_stop_sound_effects(void);
static void novel_opcode_2e_start_countdown_timer(void);
static void novel_opcode_2f_play_background_sound(void);
static void novel_opcode_30_set_master_volume(void);
static void novel_opcode_31_unload_music(void);
static void novel_opcode_32_set_menu_brightness(void);
static void novel_opcode_33_add_counter_delta_if_choice(void);
static void novel_opcode_34_wait_for_fade(void);
static void novel_opcode_35_push_operand_history(void);

/* Existing biased view retained only for opcodes 2c, 2f and 30. It aliases
 * g_novel_reading at instruction (+0x08); direct full-record accesses
 * change register allocation, while a scalar alias shortens each body.
 * The layout is a matching constraint, not evidence of a second state object. */
typedef struct novel_state {
    u32 instruction; /* 0x00 */
    u16 state_flags; /* 0x04; g_novel_state_flags; novel_state_flag_e */
    u8 _unused_06[0x0e];
    s16 render_index; /* 0x14 */
    s16 picture;      /* 0x16 */
    u8 _unused_18[0x0e];
    s16 x; /* 0x26 */
    s16 y; /* 0x28 */
} novel_state_t;
typedef char novel_state_size_must_be_44[sizeof(novel_state_t) == 44 ? 1 : -1];
extern novel_state_t g_novel_state;

/* Scalar flag access retains the original bases in opcodes 2e and 33. */
extern u16 g_novel_state_flags;
extern u32 g_novel_local_flags[];
extern s32 g_novel_wait_counter;
extern s32 g_novel_wait_frames;
extern s32 g_novel_countdown_section;
static s32 s_novel_countdown_elapsed;

/* Opcode-indexed dispatch, defined after its handlers below. */
static void (*s_novel_opcode_handlers[54])(void);

/* Sound IDs 1 through 56 select four-byte rows in the WLDCORE sound bank. */
static u8 s_novel_sound_quads[56][4] = { 1, 2, 0, 0, 3, 4, 0, 0, 5, 6, 0, 0, 7, 0, 0, 0, 8, 0, 0, 0, 9, 0, 0, 0, 0xa, 0,
    0, 0, 0xb, 0xc, 0, 0, 0xd, 0xe, 0, 0, 0xf, 0x10, 0, 0, 0x11, 0x12, 0, 0, 0x13, 0x14, 0, 0, 0x15, 0x16, 0, 0, 0x17,
    0, 0, 0, 0x18, 0x19, 0, 0, 0x1a, 0, 0, 0, 0x1b, 0x1c, 0, 0, 0x1d, 0x1e, 0, 0, 0x1f, 0x20, 0x21, 0, 0x22, 0x23, 0, 0,
    0x24, 0x25, 0, 0, 0x26, 0, 0, 0, 0x27, 0, 0, 0, 0x28, 0, 0, 0, 0x29, 0, 0, 0, 0x2a, 0, 0, 0, 0x2b, 0, 0, 0, 0x2c, 0,
    0, 0, 0x2d, 0, 0, 0, 0x2e, 0, 0, 0, 0x2f, 0, 0, 0, 0x30, 0, 0, 0, 0x31, 0, 0, 0, 0x32, 0, 0, 0, 0x33, 0, 0, 0, 0x34,
    0, 0, 0, 0x35, 0x36, 0, 0, 0x37, 0x38, 0, 0, 0x39, 0, 0, 0, 0x3a, 0x3b, 0x3c, 0, 0x3d, 0x3e, 0x3f, 0, 0x40, 0, 0, 0,
    0x41, 0, 0, 0, 0x42, 0, 0, 0, 0x43, 0, 0, 0, 0x44, 0, 0, 0, 0x45, 0, 0, 0, 0x46, 0x47, 0, 0, 0x46, 0x47, 0x48, 0x49,
    0x48, 0x49, 0, 0, 0x4a, 0x4b, 0, 0, 0x4c, 0, 0, 0, 0x4d, 0, 0, 0, 0x4e, 0, 0, 0, 0x4f, 0, 0, 0, 0x50, 0, 0, 0 };

static void branch_on_choice(s32 branch_if_equal);
static void branch_on_local_flag(s32 expected);
static void branch_on_saved_flag(s32 expected);
static void branch_on_section_read(s32 expected);
static void reset_reading(novel_reading_t* record);
static void change_reading_music(s32 music_id);
static s32 update_choice_timer(void);
static void play_pending_sounds(s32 sound_id, s32 weather_sound_id, s32 track);
static void play_background_sound(s32 sound_id);
static void play_sound_quad(s32 sound_id);

/* Resume the selected sound novel from its saved reading record (a reset one for
 * indexes past the five saved slots): restore its picture, background, music, sound, windows,
 * value display and brightness, load its files and resume at the saved section, or
 * flag a branch when there is none. */
void novel_restore_reading(s32 render_record) {
    u32* word;
    s32 i;
    s32 picture_id;
    s32 transition;
    s16 sound;
    s32 tint;
    s32 section;
    u16* state_flags;

    if (g_novel_selected < 5) {
        g_novel_reading = g_novel_saved_readings[g_novel_selected];
    } else {
        reset_reading(&g_novel_reading);
    }

    g_novel_reading.picture_render_index = render_record;
    g_novel_reading.buffer = &g_novel_script;
    g_novel_reading.state_flags &= ~(NOVEL_STATE_BRANCH | NOVEL_STATE_FETCH);
    for (i = 0; i < 1; i++) {
        g_novel_local_flags[i] = 0;
    }

    script_variable_set(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].priority = 0xC;
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].flags &= ~DISPLAY_OBJECT_POP_IN;

    picture_id = g_novel_reading.picture_id;
    if (picture_id != -1) {
        g_novel_reading.picture_id = -1;
        g_novel_reading.instruction
            = (g_novel_reading.picture_x << 24) | (g_novel_reading.picture_y << 16) | (picture_id << 8);
        novel_opcode_0a_load_picture();
    }

    transition = g_novel_reading.background_set;
    if (transition != -1) {
        g_novel_reading.instruction = transition << 8;
        g_novel_reading.background_set = -1;
        g_picture_background_loaded_set = -1;
        novel_opcode_05_load_background_set();
    }

    if (g_novel_reading.music_selection & 0xFF) {
        music_queue_start_track(g_novel_reading.music_selection);
    } else {
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
        music_queue_add(MUSIC_QUEUE_COMMAND_UNLOAD, 2);
        g_novel_reading.music_selection = 0;
    }

    /* Bit 0x4000 marks an environment sound (novel_opcode_2f_play_background_sound);
     * otherwise the id names a sound quad. */
    sound = g_novel_reading.sound_id;
    if (sound != 0) {
        if (sound & 0x4000) {
            play_background_sound((sound ^ 0x4000));
        } else {
            play_sound_quad(sound);
        }
    }

    state_flags = &g_novel_reading.state_flags;
    if (*state_flags & NOVEL_STATE_PARENT_WINDOWS_VISIBLE) {
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].novel.quit_prompt_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].novel.history_prompt_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
    }

    if (*state_flags & NOVEL_STATE_VALUE_DISPLAY) {
        g_novel_reading.instruction = g_novel_reading.value_display_mode << 8;
        novel_opcode_1c_show_value_display();
    }

    tint = g_novel_reading.brightness;
    if (tint == 0x100) {
        tint = 0xFF;
    }

    g_world_map_fade.boxes[1].r = tint;
    g_world_map_fade.boxes[1].g = tint;
    g_world_map_fade.boxes[1].b = tint;
    g_world_map_fade.priorities[1] = 0xB;
    music_queue_wait_until_empty();
    novel_load_files(g_novel_selected);
    file_wait_pending();
    section = g_novel_reading.section;
    if (section != 0) {
        /* The 32-bit view preserves register order; the named offset field swaps the index and base. */
        g_novel_reading.script_words = &((u32*)&g_novel_script)[((u32*)g_novel_reading.buffer)[section] / sizeof(u32)];
        /* A struct-field store: as a scalar global the store would be
         * scheduled above the table load (GCC 2.7 alias rule). */
        g_novel_reading.word_index = 0;
        *state_flags |= NOVEL_STATE_FETCH;
    } else {
        *state_flags |= NOVEL_STATE_BRANCH;
    }
}

/* Reset the selected sound novel's saved record so the next reading starts from the beginning; the
 * argument is unused. */
void novel_reset_selected_reading(s32 unused) {
    reset_reading(&g_novel_saved_readings[g_novel_selected]);
}

/* Reset a sound-novel record to the start of a reading: no text history, no
 * picture or background, no sound, section 0 and brightness 0x30. */
static void reset_reading(novel_reading_t* record) {
    record->text_history[3] = -1;
    record->text_history[2] = -1;
    record->text_history[1] = -1;
    record->text_history[0] = -1;
    record->picture_id = -1;
    record->background_set = -1;
    record->sound_mode = NOVEL_SOUND_MODE_EFFECTS_ONLY;
    record->music_selection = 0;
    record->sound_id = 0;
    record->state_flags = 0;
    record->section = 0;
    record->counter = 0;
    record->_unknown_18 = 0;
    record->text_id = 0;
    record->counter_delta = 0;
    record->brightness = 0x30;
}

/* Runs the sound novel's script until it yields. Each turn plays the sounds
 * pending in the three PENDING script variables and steps the countdown,
 * then:
 *
 *   BRANCH marks the reached section's saved-data bit (novel * 256 +
 *   section), moves on to the next section, points the instructions at that
 *   section's offset in the save-slot buffer and, for the five saved novels,
 *   copies the whole record into g_novel_saved_readings[g_novel_selected];
 *
 *   FETCH loads the next 32-bit instruction and advances the word index,
 *   setting FETCHED for the handler's first call;
 *
 *   the opcode byte then selects a handler from g_novel_opcode_handlers.
 *
 * The turn loop ends once a handler sets YIELD or END. Returns 0 when the
 * script has ended, which is how novel_update_reader tests
 * it; it also clears YIELD on the way out.
 *
 * The record holds the loaded script's section-offset table. */
s32 novel_run_script(void) {
    s32 sound_id;
    s32 weather_sound_id;
    u32 section_byte_offset;
    u16 flags;
    s32 aborted;

    do {
        sound_id = script_variable_get(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT);
        weather_sound_id = script_variable_get(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND);
        play_pending_sounds(sound_id, weather_sound_id, script_variable_get(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK));
        update_choice_timer();

        if (g_novel_reading.state_flags & NOVEL_STATE_BRANCH) {
            bits_set_value(g_novel_read_section_bits, (g_novel_selected << 8) + g_novel_reading.section, 1);
            g_novel_reading.state_flags &= ~NOVEL_STATE_BRANCH;
            section_byte_offset = g_novel_reading.buffer->offsets[g_novel_reading.section];
            g_novel_reading.section++;
            g_novel_reading.word_index = 0;
            g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
            g_novel_reading.script_words = &((u32*)&g_novel_script)[section_byte_offset / sizeof(u32)];
            if (g_novel_selected < 5) {
                memcpy(&g_novel_saved_readings[g_novel_selected], &g_novel_reading, sizeof(novel_reading_t));
            }
        }

        if (g_novel_reading.state_flags & NOVEL_STATE_FETCH) {
            g_novel_reading.state_flags = (g_novel_reading.state_flags & ~NOVEL_STATE_FETCH) | NOVEL_STATE_FETCHED;
            g_novel_reading.instruction = g_novel_reading.script_words[g_novel_reading.word_index];
            g_novel_reading.word_index++;
        }

        s_novel_opcode_handlers[(u8)g_novel_reading.instruction]();

        flags = g_novel_reading.state_flags;
        g_novel_reading.state_flags = flags & ~NOVEL_STATE_FETCHED;
    } while ((flags & (NOVEL_STATE_YIELD | NOVEL_STATE_END)) == 0);

    aborted = g_novel_reading.state_flags & NOVEL_STATE_END;
    g_novel_reading.state_flags &= ~NOVEL_STATE_YIELD;
    return aborted == 0;
}

/* Find an opcode in a sound-novel script section; return its whole instruction or zero. */
u32 novel_find_section_instruction(s32 section, s32 opcode) {
    u32* word = &((u32*)&g_novel_script)[g_novel_reading.buffer->offsets[section] / sizeof(u32)];
    s32 i;

    for (i = 0; i < 256; i++) {
        u32 value = *word;
        if (opcode == (value & 0xFF)) {
            return value;
        }
        if ((value & 0xFF) == 1) {
            return 0;
        }
        word++;
    }
    return 0;
}

/* Clear a render record's text image, draw text `text_id` centered
 * horizontally in it (nothing for id 0) and upload it. Also clears the
 * three pending sound script variables. */
void window_draw_centered_text(s32 window_index, s32 text_id) {
    point32_t point;
    s16 text_width;
    s16 text_height;
    s32 i;

    text_measure_pixels(&text_width, &text_height, text_find_entry(text_id));
    point.x = (((g_display_object_gs_image_pool[window_index].bounds.w - text_width) / 2) & 0xfffe) + 8;
    point.y = 10;
    for (i = 0; i < g_display_object_gs_image_pool[window_index].bounds.w
            * g_display_object_gs_image_pool[window_index].bounds.h / 8;
        i++) {
        g_window_image_buffer[i] = 0;
    }
    if (text_id != 0) {
        window_display_text_entry(window_index, text_id, point, g_window_image_buffer);
    }
    window_load_render_record_image(window_index, g_window_image_buffer);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}

/* Consume the three pending audio script variables according to the reading's
 * sound mode, then clear them. The interpreter calls this before dispatching
 * each instruction, including repeated calls to a yielding handler. */
static void play_pending_sounds(s32 sound_id, s32 weather_sound_id, s32 track) {
    s32* music_flags;

    if ((sound_id | weather_sound_id | track) == 0) {
        return;
    }
    switch (g_novel_reading.sound_mode) {
    case NOVEL_SOUND_MODE_EFFECTS_ONLY:
        if (sound_id != SOUND_SFX_NONE) {
            sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            play_sound_quad(weather_sound_id);
        }
        break;
    case NOVEL_SOUND_MODE_EFFECTS_AND_MUSIC:
        if (sound_id != SOUND_SFX_NONE) {
            sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            play_sound_quad(weather_sound_id);
        }
        if (track != 0) {
            if ((g_novel_reading.music_selection & 0xFF) != track) {
                change_reading_music(track);
                music_flags = &g_music_queue.music.flags;
                *music_flags |= MUSIC_QUEUE_PLAYING;
                music_switch_track(g_novel_reading.music_selection >> 8, 0, 0);
                music_set_target_volume(g_music_queue.volume, 0x28);
            }
        }
        break;
    }
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_variable_set(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}

enum { SOUND_QUAD_COMMAND_COUNT = 4, SOUND_QUAD_COMMAND_BASE = SUZUKI_SOUND_ID(SOUND_RESOURCE_SOUND_NOVEL, 0) };

/* Stops all sound effects and plays the row for sound_id from
 * s_novel_sound_quads: four sounds of the sound novel's
 * resource, one on each SFX channel pair. A zero byte selects that
 * resource's empty sound 0, which only stops its channel pair. */
static void play_sound_quad(s32 sound_id) {
    s32 values[SOUND_QUAD_COMMAND_COUNT];
    s32 i;

    sound_stop_all_sfx();
    for (i = 0; i < SOUND_QUAD_COMMAND_COUNT; i++) {
        values[i] = s_novel_sound_quads[sound_id - 1][i] + SOUND_QUAD_COMMAND_BASE;
    }
    suzuki_sound_play_4_sfx(values[0], values[1], values[2], values[3]);
}

/* Stop all sound effects and play environment sound `sound_id` as the
 * background sound. */
static void play_background_sound(s32 sound_id) {
    sound_stop_all_sfx();
    sound_play_background(sound_id + SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, 0));
}

/* Same 32-bit value as g_novel_countdown_frames; the target stores 0 through
 * both spellings, and a single name lets GCC merge the two stores. */
extern s32 g_novel_countdown_frames_reset;

/*
 * Steps the sound novel's choice countdown. Returns 1 on the frame the script
 * is redirected to the timeout section, 0 otherwise.
 *
 * While a countdown is set, each call counts one frame and, with the play
 * time display selected, shows the time remaining (60 frames to the second).
 * When it runs out the message thread is told to close (task 3) and the
 * countdown becomes -1; once that thread has ended the script branches to
 * g_novel_countdown_section.
 */
static s32 update_choice_timer(void) {
    s32 elapsed;
    s32 remaining;
    s32 minutes;
    s32 hours;

    if (g_novel_countdown_frames == -1) {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return 0;
        }
        g_novel_countdown_frames = 0;
        g_novel_countdown_frames_reset = 0;
        g_novel_reading.section = g_novel_countdown_section;
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
        return 1;
    }

    if (g_novel_countdown_frames != 0) {
        elapsed = s_novel_countdown_elapsed + 1;
        s_novel_countdown_elapsed = elapsed;
        remaining = g_novel_countdown_frames - elapsed;
        if (g_hud_value_mode == HUD_VALUE_PLAY_TIME) {
            hours = remaining / 216000;
            minutes = remaining / 3600;
            g_hud_value_secondary = minutes - hours * 60;
            g_hud_value_tertiary = remaining / 60 - minutes * 60;
            g_hud_value = hours % 24;
        }
        if (g_novel_countdown_frames == elapsed) {
            menu_screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
            g_novel_countdown_frames = -1;
        }
    }
    return 0;
}

/* Sound-novel opcode 0x00: end the script; novel_run_script then returns 0. */
static void novel_opcode_00_end_script(void) {
    g_novel_reading.state_flags |= NOVEL_STATE_END;
}

/* Sound-novel opcode: end the section; the script branches to the recorded
 * section after yielding this frame. */
static void novel_opcode_01_end_section(void) {
    g_novel_reading.state_flags |= NOVEL_STATE_BRANCH | NOVEL_STATE_YIELD;
}

/* Sound-novel opcode 0x02: do nothing and fetch the next instruction. */
static void novel_opcode_02_nop(void) {
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x03: show message byte 1 of the novel's text
 * (TEXT_ID_WORLD_MESSAGE_BASE | byte 1) in the message box, as
 * novel_opcode_0d_start_text_thread does, then push it onto the four-entry
 * history g_novel_reading.text_history[0..3]. */
static void novel_opcode_03_start_text_thread_and_push_history(void) {
    u16 history_1;
    u16 history_2;
    u16 history_3;
    u16 flags;
    s32 operand = (u8)(g_novel_reading.instruction >> 8);

    thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_dialog_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    history_1 = g_novel_reading.text_history[1];
    history_2 = g_novel_reading.text_history[2];
    history_3 = g_novel_reading.text_history[3];
    flags = g_novel_reading.state_flags;
    g_novel_reading.text_id = operand;
    g_novel_reading.text_history[3] = operand;
    g_novel_reading.text_history[0] = history_1;
    g_novel_reading.text_history[1] = history_2;
    g_novel_reading.text_history[2] = history_3;
    g_novel_reading.state_flags = flags | NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: yield while the message thread is running. */
static void novel_opcode_04_wait_for_text_thread(void) {
    if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_YIELD;
    }
}

/* Sound-novel opcode: load background set operand 0 unless it is the one
 * already recorded, waiting for the file. */
static void novel_opcode_05_load_background_set(void) {
    s32 value = (u8)(g_novel_reading.instruction >> 8);

    if (g_novel_reading.background_set != value) {
        DrawSync(0);
        g_novel_reading.background_set = value;
        world_map_load_background_picture_set(value);
        file_wait_pending();
    }
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x06: branch to the section in instruction byte 1. */
static void novel_opcode_06_branch_to_section(void) {
    g_novel_reading.section = (u8)(g_novel_reading.instruction >> 8);
    g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
}

/* Sound-novel opcode 0x07: branch to the section in byte 1 when the chosen dialog option equals
 * byte 2. */
static void novel_opcode_07_branch_if_dialog_option_equal(void) {
    branch_on_choice(1);
}

/* Branches the sound-novel script to section byte 1 when the selected dialog
 * option equals byte 2 (or differs from it, when `branch_if_equal` is 0);
 * otherwise fetches the next instruction. */
static void branch_on_choice(s32 branch_if_equal) {
    u8 operand = (g_novel_reading.instruction >> 16);

    if ((operand == script_variable_get(EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION)) == branch_if_equal) {
        g_novel_reading.section = (u8)(g_novel_reading.instruction >> 8);
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    }
}

/* Sound-novel opcode 0x08: branch to the section in byte 1 when the chosen dialog option differs
 * from byte 2. */
static void novel_opcode_08_branch_if_dialog_option_not_equal(void) {
    branch_on_choice(0);
}

/* Sound-novel opcode 0x09: yield every frame until the frame counter passes byte 1, then fetch the
 * next instruction (byte 1 + 2 frames in all). */
static void novel_opcode_09_wait_frames(void) {
    u16 flags = g_novel_reading.state_flags;
    u16 updated = flags | NOVEL_STATE_YIELD;
    s32 counter;
    s32 operand;

    g_novel_reading.state_flags = updated;
    if ((updated & NOVEL_STATE_FETCHED) != 0) {
        g_novel_wait_counter = 0;
        g_novel_wait_frames = (u8)(g_novel_reading.instruction >> 8);
    }
    counter = g_novel_wait_counter;
    operand = g_novel_wait_frames;
    if (operand < counter) {
        g_novel_reading.state_flags = flags | (NOVEL_STATE_FETCH | NOVEL_STATE_YIELD);
    } else {
        g_novel_wait_counter = counter + 1;
    }
}

/* Sound-novel opcode: loads picture byte 1 through
 * world_map_load_picture_set, places its render record at
 * (byte 3 - 0x80, byte 2 - 0x78), uploads the TIM's pixels and CLUT, and
 * binds the render record to the VRAM image. */
static void novel_opcode_0a_load_picture(void) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t unused_slot; /* reserves the target frame's 0x50 slot */
    u32 instruction;
    s32 picture;
    s32 x;
    s32 y;

    instruction = g_novel_reading.instruction;
    x = instruction >> 24;
    y = (instruction >> 16) & 0xFF;
    picture = (instruction & 0xFF00) >> 8;
    g_novel_reading.picture_id = picture;
    g_novel_reading.picture_x = x;
    g_novel_reading.picture_y = y;
    world_map_load_picture_set(picture);
    file_wait_pending();
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].position.x = x - 0x80;
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].position.y = y - 0x78;
    GsGetTimInfo(g_picture_buffer + 1, &image);
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut_position.x = image.cx;
    clut_position.y = image.cy;
    LoadTPage(image.pixel, 0, 0, bounds.x, bounds.y, bounds.w * 4, bounds.h);
    LoadClut(image.clut, clut_position.x, clut_position.y);
    display_object_init_gs_image(g_novel_reading.picture_render_index, bounds, clut_position, DISPLAY_OBJECT_8BIT_CLUT);
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].pop_in_step = 0;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Hides the sound-novel picture's render record and forgets the picture. */
static void novel_opcode_0b_hide_picture(void) {
    g_display_object_gs_image_pool[g_novel_reading.picture_render_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_novel_reading.picture_id = -1;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH | NOVEL_STATE_YIELD;
}

/* Sound-novel opcode: a nonzero byte 1 shows the novel level's two button
 * windows and records PARENT_WINDOWS_VISIBLE, zero hides them and clears it. */
static void novel_opcode_0c_set_parent_windows_visible(void) {
    if ((u8)(g_novel_reading.instruction >> 8) != 0) {
        g_novel_reading.state_flags |= NOVEL_STATE_PARENT_WINDOWS_VISIBLE;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].novel.quit_prompt_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].novel.history_prompt_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_novel_reading.state_flags &= ~NOVEL_STATE_PARENT_WINDOWS_VISIBLE;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].novel.quit_prompt_window].flags
            |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].novel.history_prompt_window].flags
            |= DISPLAY_OBJECT_HIDDEN;
    }
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x0D: show message byte 1 of the novel's text in the message thread and
 * record it, without the history push of opcode 0x03. */
static void novel_opcode_0d_start_text_thread(void) {
    s32 operand = (u8)(g_novel_reading.instruction >> 8);

    thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_dialog_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    g_novel_reading.text_id = operand;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: branch to one of the three sections named by the
 * operand bytes, chosen by rand() with equal weight. */
static void novel_opcode_0e_branch_to_random_section(void) {
    s32 pick = (rand() * 3) >> 15;
    s32 value;

    if (pick == 0) {
        value = (u8)(g_novel_reading.instruction >> 8);
    }
    if (pick == 1) {
        value = (u8)(g_novel_reading.instruction >> 16);
    }
    if (pick == 2) {
        value = (g_novel_reading.instruction >> 24);
    }
    g_novel_reading.section = value;
    g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
}

/* Sound-novel opcode 0x0F: branch to the section in byte 3 when section byte 2 of novel byte 1 has
 * been reached, i.e. bit (byte 1 << 8 | byte 2) of g_novel_read_section_bits that
 * novel_run_script sets on each branch. */
static void novel_opcode_0f_branch_if_section_read(void) {
    branch_on_section_read(1);
}

/* Branch to section byte 3 when bit (byte 1 << 8 | byte 2) of
 * g_novel_read_section_bits equals `expected`; otherwise fetch the next
 * instruction. */
static void branch_on_section_read(s32 expected) {
    u32* instruction = &g_novel_reading.instruction;
    u16 flags;

    if (bits_test(g_novel_read_section_bits, ((u8)(*instruction >> 8) << 8) | (u8)(*instruction >> 16)) == expected) {
        flags = g_novel_reading.state_flags | NOVEL_STATE_BRANCH;
        g_novel_reading.section = (*instruction >> 24);
    } else {
        flags = g_novel_reading.state_flags | NOVEL_STATE_FETCH;
    }
    g_novel_reading.state_flags = flags;
}

/* Sound-novel opcode 0x10: branch to the section in byte 3 when section byte 2 of novel byte 1 has
 * not been reached. */
static void novel_opcode_10_branch_if_section_unread(void) {
    branch_on_section_read(0);
}

/* Sound-novel opcode 0x11: branch to the section in byte 2 when saved flag byte 1
 * (g_novel_saved_flags) is set. */
static void novel_opcode_11_branch_if_saved_flag_set(void) {
    branch_on_saved_flag(1);
}

/* Branch to section byte 2 when bit byte 1 of g_novel_saved_flags
 * equals `expected`; otherwise fetch the next instruction. */
static void branch_on_saved_flag(s32 expected) {
    u32* instruction = &g_novel_reading.instruction;
    u16 flags;

    /* Keep the instruction address live across the call for both operand loads. */
    if (bits_test(g_novel_saved_flags, (u8)(*instruction >> 8)) == expected) {
        flags = g_novel_reading.state_flags | NOVEL_STATE_BRANCH;
        g_novel_reading.section = (u8)(*instruction >> 16);
    } else {
        flags = g_novel_reading.state_flags | NOVEL_STATE_FETCH;
    }
    g_novel_reading.state_flags = flags;
}

/* Sound-novel opcode 0x12: branch to the section in byte 2 when saved flag byte 1 is clear. */
static void novel_opcode_12_branch_if_saved_flag_clear(void) {
    branch_on_saved_flag(0);
}

/* Sound-novel opcode 0x13: branch to the section in byte 2 when local flag byte 1 is set; the
 * local flags are cleared whenever a reading starts or resumes. */
static void novel_opcode_13_branch_if_local_flag_set(void) {
    branch_on_local_flag(1);
}

/* Branch to section byte 2 when bit byte 1 of g_novel_local_flags
 * equals `expected`; otherwise fetch the next instruction. */
static void branch_on_local_flag(s32 expected) {
    u32* instruction = &g_novel_reading.instruction;
    u16 flags;

    if (bits_test(g_novel_local_flags, (u8)(*instruction >> 8)) == expected) {
        flags = g_novel_reading.state_flags | NOVEL_STATE_BRANCH;
        g_novel_reading.section = (u8)(*instruction >> 16);
    } else {
        flags = g_novel_reading.state_flags | NOVEL_STATE_FETCH;
    }
    g_novel_reading.state_flags = flags;
}

/* Sound-novel opcode 0x14: branch to the section in byte 2 when local flag byte 1 is clear. */
static void novel_opcode_14_branch_if_local_flag_clear(void) {
    branch_on_local_flag(0);
}

/* Sound-novel opcode 0x15: set saved flag byte 1 (g_novel_saved_flags). */
static void novel_opcode_15_set_saved_flag(void) {
    bits_set_value(g_novel_saved_flags, (u8)(g_novel_reading.instruction >> 8), 1);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x16: clear saved flag byte 1. */
static void novel_opcode_16_clear_saved_flag(void) {
    bits_set_value(g_novel_saved_flags, (u8)(g_novel_reading.instruction >> 8), 0);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x17: set local flag byte 1 (kept only for the current reading). */
static void novel_opcode_17_set_local_flag(void) {
    bits_set_value(g_novel_local_flags, (u8)(g_novel_reading.instruction >> 8), 1);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x18: clear local flag byte 1. */
static void novel_opcode_18_clear_local_flag(void) {
    bits_set_value(g_novel_local_flags, (u8)(g_novel_reading.instruction >> 8), 0);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x19: set the record's counter to the 16-bit operand in bytes 1-2. */
static void novel_opcode_19_set_counter(void) {
    g_novel_reading.counter = (g_novel_reading.instruction << 8) >> 16;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: add the 16-bit operand (instruction bits 8..23) to the
 * record's counter delta. */
static void novel_opcode_1a_add_counter_delta(void) {
    g_novel_reading.counter_delta += (g_novel_reading.instruction << 8) >> 16;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: subtract the 16-bit operand (instruction bits 8..23)
 * from the record's counter delta. */
static void novel_opcode_1b_subtract_counter_delta(void) {
    g_novel_reading.counter_delta -= (g_novel_reading.instruction << 8) >> 16;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: show the context value display in the mode of operand
 * 0 at (-0x20, 0x54). Any mode but the play time shows the record's counter. */
static void novel_opcode_1c_show_value_display(void) {
    s32 mode;

    g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    mode = (u8)(g_novel_reading.instruction >> 8);
    g_novel_reading.value_display_mode = mode;
    g_hud_value_mode = mode;
    if (mode != HUD_VALUE_PLAY_TIME) {
        g_hud_value = g_novel_reading.counter;
    }
    g_hud_value_position.x = -0x20;
    g_hud_value_position.y = 0x54;
    g_hud_ot_priority = 0xa;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH | NOVEL_STATE_VALUE_DISPLAY;
}

/* Opcode 29: take the numeric value display down again (novel_opcode_1c_show_value_display
 * puts it up) and fetch the next instruction. */
static void novel_opcode_1d_hide_value_display(void) {
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    g_novel_reading.state_flags = (g_novel_reading.state_flags & ~NOVEL_STATE_VALUE_DISPLAY) | NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x1E: add the counter delta to the counter, clamp it to 0..0xFFFF, clear the
 * delta and show the counter in the value display. */
static void novel_opcode_1e_apply_counter_delta(void) {
    s32 delta;
    s32* value = &g_novel_reading.counter;

    delta = g_novel_reading.counter_delta;
    g_novel_reading.counter_delta = 0;
    *value += delta;
    if (*value > 0xFFFF) {
        *value = 0xFFFF;
    }
    if (*value < 0) {
        *value = 0;
    }
    g_hud_value = *value;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x1F: play sound effect byte 1 on a free channel. */
static void novel_opcode_1f_play_sound(void) {
    sound_play_sfx_on_free_channel((u8)(g_novel_reading.instruction >> 8));
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x20: switch the novel's music to track byte 1. */
static void novel_opcode_20_load_music(void) {
    change_reading_music((u8)(g_novel_reading.instruction >> 8));
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Replace the sound novel's music with `music_id`: stop the playing track,
 * unload its slot (slot 2 is used when none is loaded) and start the new
 * track; the load-wait loop after it polls a stub that always returns 0. The saved record keeps the slot in the high
 * byte and the music id in the low byte. */
static void change_reading_music(s32 music_id) {
    s16 packed = g_novel_reading.music_selection;
    s32 slot = packed >> 8;

    if ((packed & 0xFF) != 0) {
        music_stop();
    }
    if (slot != 0) {
        music_unload_scenario_slot(slot);
    } else {
        slot = 2;
    }
    VSync(2);
    music_load_into_slot(music_id, slot);
    g_novel_reading.music_selection = (slot << 8) | music_id;
    while (music_is_loading_stub() != 0) {
        VSync(0);
    }
    g_music_queue.music.tracks[slot - 1] = music_id;
    g_music_queue.current_slot = slot;
}

/* Sound-novel opcode 0x21: set the record's sound mode to byte 1; in mode 1 the pending-music
 * script variable also changes the track, mode 0 plays only effects and weather sounds. */
static void novel_opcode_21_set_sound_mode(void) {
    g_novel_reading.sound_mode = (u8)(g_novel_reading.instruction >> 8);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: branch to section byte 3 when the counter plus its
 * pending delta, clamped to 0..0xffff, equals the 16-bit operand in bytes 1-2.
 * The double negation keeps the native counter/delta add operand order. */
static void novel_opcode_22_branch_if_equal(void) {
    s32 value = g_novel_reading.counter - (-g_novel_reading.counter_delta);
    s32 operand;
    u32 word;

    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_novel_reading.instruction;
    operand = (word >> 8) & 0xffff;
    if (value == operand) {
        g_novel_reading.section = word >> 24;
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    }
}

/* As novel_opcode_22_branch_if_equal, branching when the values differ. */
static void novel_opcode_23_branch_if_not_equal(void) {
    s32 value = g_novel_reading.counter - (-g_novel_reading.counter_delta);
    s32 operand;
    u32 word;

    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_novel_reading.instruction;
    operand = (word >> 8) & 0xffff;
    if (value != operand) {
        g_novel_reading.section = word >> 24;
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    }
}

/* As novel_opcode_22_branch_if_equal, branching when the counter is below the
 * operand. */
static void novel_opcode_24_branch_if_less(void) {
    s32 value = g_novel_reading.counter - (-g_novel_reading.counter_delta);
    s32 operand;
    u32 word;

    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_novel_reading.instruction;
    operand = (word >> 8) & 0xffff;
    if (value < operand) {
        g_novel_reading.section = word >> 24;
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    }
}

/* As novel_opcode_22_branch_if_equal, branching when the counter is above the
 * operand. */
static void novel_opcode_25_branch_if_greater(void) {
    s32 value = g_novel_reading.counter - (-g_novel_reading.counter_delta);
    s32 operand;
    u32 word;

    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_novel_reading.instruction;
    operand = (word >> 8) & 0xffff;
    if (operand < value) {
        g_novel_reading.section = word >> 24;
        g_novel_reading.state_flags |= NOVEL_STATE_BRANCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    }
}

/* Sound-novel opcode: fade the music to silence over operand 0 * 3 steps and
 * clear the playing flag and the selected music id (the slot is kept). */
static void novel_opcode_26_fade_out_music(void) {
    s32 operand;

    g_music_queue.music.flags &= ~MUSIC_QUEUE_PLAYING;
    operand = (u8)(g_novel_reading.instruction >> 8);
    music_set_target_volume(0, operand * 3);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    g_novel_reading.music_selection &= 0xFF00;
}

/* Sound-novel opcode: when a track is selected, switch playback to its slot
 * at volume 0 and fade to the queue's volume over operand 0 * 3 steps. */
static void novel_opcode_27_switch_music_track(void) {
    s16 track = g_novel_reading.music_selection;
    s32* state;
    s32 operand;

    if (track != 0) {
        state = &g_music_queue.music.flags;
        *state |= MUSIC_QUEUE_PLAYING;
        operand = (u8)(g_novel_reading.instruction >> 8);
        music_switch_track(track >> 8, 0, 0);
        music_set_target_volume(g_music_queue.volume, operand * 3);
    }
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: fade the screen out over operand 0 frames. The
 * fade-out bit is cleared first so that world_map_start_fade always
 * starts the fade; the box-visible bit set here is replaced by it. */
static void novel_opcode_28_fade_screen_out(void) {
    g_world_map_fade.flags[0] = (g_world_map_fade.flags[0] & ~WORLD_MAP_FADE_OUT) | WORLD_MAP_FADE_BOX_VISIBLE;
    world_map_start_fade(WORLD_MAP_FADE_OUT, (u8)(g_novel_reading.instruction >> 8));
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: fade the screen in over operand 0 frames and drop the
 * white-overlay state. */
static void novel_opcode_29_fade_screen_in(void) {
    u32* state = &g_world_map_fade.flags[0];
    u16* flags = &g_novel_reading.state_flags;

    *state |= WORLD_MAP_FADE_OUT;
    *flags &= ~NOVEL_STATE_WHITE_OVERLAY;
    world_map_start_fade(0, (u8)(g_novel_reading.instruction >> 8));
    *flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 42 of g_novel_opcode_handlers. Drains the GPU and
 * waits a frame, then with byte 1 zero puts up the full-screen white overlay
 * (fade box white and visible, any fade stopped, WHITE_OVERLAY recorded, the
 * menus blacked out), or otherwise takes it down again (box black and hidden,
 * menus back to neutral 0x80 brightness). Yields for the frame.
 *
 * The menu_screen_set_brightness call and the 0xc store are written once per
 * arm; jump.c cross-jumps the common tail from the g_novel_reading.state_flags store
 * onward, leaving only the three argument constants in the arms.
 */
static void novel_opcode_2a_set_white_screen_overlay(void) {
    u16* flags;

    DrawSync(0);
    VSync(0);
    if ((u8)(g_novel_reading.instruction >> 8) == 0) {
        u32* state = &g_world_map_fade.flags[0];

        g_world_map_fade.boxes[0].r = 0xff;
        g_world_map_fade.boxes[0].g = 0xff;
        g_world_map_fade.boxes[0].b = 0xff;
        g_world_map_flags &= ~WORLD_MAP_FLAG_FADING;
        *state = (*state & ~(WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_OUT)) | WORLD_MAP_FADE_BOX_VISIBLE;
        g_novel_reading.state_flags |= NOVEL_STATE_WHITE_OVERLAY;
        menu_screen_set_brightness(0, 0, 0);
    } else {
        u32* state = &g_world_map_fade.flags[0];

        g_world_map_fade.boxes[0].r = 0;
        g_world_map_fade.boxes[0].g = 0;
        g_world_map_fade.boxes[0].b = 0;
        g_world_map_flags &= ~WORLD_MAP_FLAG_FADING;
        *state &= ~(WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_OUT | WORLD_MAP_FADE_BOX_VISIBLE);
        g_novel_reading.state_flags &= ~NOVEL_STATE_WHITE_OVERLAY;
        menu_screen_set_brightness(0x80, 0x80, 0x80);
    }
    flags = &g_novel_reading.state_flags;
    *flags = *flags | (NOVEL_STATE_FETCH | NOVEL_STATE_YIELD);
}

/* Sound-novel opcode: yield each frame until Circle is pressed.
 *
 * Each block takes its own pointer local so the flag address is materialized
 * twice, as in the target. */
static void novel_opcode_2b_wait_for_circle_press(void) {
    if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        u16* state_flags;

        state_flags = &g_novel_reading.state_flags;
        *state_flags |= NOVEL_STATE_FETCH;
    }
    {
        u16* state_flags;

        state_flags = &g_novel_reading.state_flags;
        *state_flags |= NOVEL_STATE_YIELD;
    }
}

/* Sound-novel opcode: stop all sound effects and play sound quad byte 1. A
 * nonzero byte 2 also records the sound so a restored reading replays it. */
static void novel_opcode_2c_play_sound_effect_quad(void) {
    char unused[8];
    s32 sound_id;

    sound_id = (u8)(g_novel_reading.instruction >> 8);
    if (g_novel_state.instruction & 0xFF0000) {
        g_novel_reading.sound_id = sound_id;
    }
    play_sound_quad(sound_id);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: stop all sound effects and clear the recorded sound. */
static void novel_opcode_2d_stop_sound_effects(void) {
    sound_stop_all_sfx();
    g_novel_reading.sound_id = 0;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: start a countdown of the 16-bit operand in seconds
 * (60 frames each); when it runs out the script branches to the section in
 * the top operand byte. */
static void novel_opcode_2e_start_countdown_timer(void) {
    u32 instruction;

    instruction = g_novel_reading.instruction;
    s_novel_countdown_elapsed = 0;
    g_novel_countdown_frames = ((instruction << 8) >> 16) * 60;
    g_novel_countdown_section = instruction >> 24;
    g_novel_state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: stop all sound effects and play environment sound
 * operand 0 as the background sound. A nonzero operand 1 also records the
 * sound, flagged 0x4000, so a restored reading replays it. */
static void novel_opcode_2f_play_background_sound(void) {
    char unused[8];
    s32 sound_id;

    sound_id = (u8)(g_novel_reading.instruction >> 8);
    if (g_novel_state.instruction & 0xFF0000) {
        g_novel_reading.sound_id = sound_id | 0x4000;
    }
    play_background_sound(sound_id);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: set the master volume to operand 0 * 0x40 (0xff maps
 * to the maximum 0x3fff) over operand 1 * 3 steps, at least one. */
static void novel_opcode_30_set_master_volume(void) {
    s32 unused[2];
    s32 balance = (u8)(g_novel_reading.instruction >> 16) * 3;
    s32 volume = (g_novel_state.instruction & 0xFF00) >> 2;

    if (balance == 0) {
        balance = 1;
    }
    if (volume == 0x3FC0) {
        volume = 0x3FFF;
    }
    suzuki_sound_set_master_volume(volume, balance);
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Script opcode: stop the pending music track, clear its state slot, and mark
 * the opcode complete. */
static void novel_opcode_31_unload_music(void) {
    s16* music = &g_novel_reading.music_selection;
    music_queue_slots_t* state;
    s32* tracks;

    if (*music != 0) {
        state = &g_music_queue.music;
        state->flags &= ~MUSIC_QUEUE_PLAYING;
        music_stop();
        music_unload_scenario_slot(*music >> 8);
        /* Retail bug: change_reading_music records the track at
         * tracks[slot - 1], but this clears tracks[slot], one entry past it. */
        tracks = state->tracks;
        tracks[*music >> 8] = 0;
        g_music_queue.current_slot = 0;
        *music = 0;
    }
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode 0x32: set WORLD's menu brightness to red, green and blue bytes 1-3. */
static void novel_opcode_32_set_menu_brightness(void) {
    menu_screen_set_brightness((u8)(g_novel_reading.instruction >> 8), (u8)(g_novel_reading.instruction >> 16),
        (g_novel_reading.instruction >> 24));
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Scalar counter access preserves the separate load/store symbol bases. */
extern u16 g_novel_counter_delta;

/* Sound-novel opcode 0x33: when EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION equals
 * byte 3, add bytes 1-2 minus 0x8000 (an operand biased by 0x8000) to
 * g_novel_counter_delta. */
static void novel_opcode_33_add_counter_delta_if_choice(void) {
    u32* instruction;
    u8 expected;

    instruction = &g_novel_reading.instruction;

    expected = *instruction >> 24;
    if (script_variable_get(EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION) == expected) {
        g_novel_counter_delta += (s32)((*instruction << 8) >> 16) - 0x8000;
    }
    g_novel_state_flags |= NOVEL_STATE_FETCH;
}

/* Sound-novel opcode: yield while a screen fade is running. */
static void novel_opcode_34_wait_for_fade(void) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_FADING) == 0) {
        g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
    } else {
        g_novel_reading.state_flags |= NOVEL_STATE_YIELD;
    }
}

/* Sound-novel opcode: shift the four-entry history
 * g_novel_reading.text_history down one slot and push byte 1 at
 * the end (same shape as novel_opcode_03_start_text_thread_and_push_history). */
static void novel_opcode_35_push_operand_history(void) {
    u16 history_1 = g_novel_history_1;
    u8 new_entry = (g_novel_reading.instruction & 0xff00) >> 8;
    u16 history_2 = (u16)g_novel_reading.text_history[2];
    u16 history_3 = (u16)g_novel_reading.text_history[3];

    g_novel_reading.text_history[0] = history_1;
    g_novel_history_1 = history_2;
    g_novel_reading.text_history[2] = history_3;
    g_novel_reading.text_history[3] = new_entry;
    g_novel_reading.state_flags |= NOVEL_STATE_FETCH;
}

/* Dispatch by the instruction's low-byte opcode. */
static void (*s_novel_opcode_handlers[54])(void) = {
    novel_opcode_00_end_script,
    novel_opcode_01_end_section,
    novel_opcode_02_nop,
    novel_opcode_03_start_text_thread_and_push_history,
    novel_opcode_04_wait_for_text_thread,
    novel_opcode_05_load_background_set,
    novel_opcode_06_branch_to_section,
    novel_opcode_07_branch_if_dialog_option_equal,
    novel_opcode_08_branch_if_dialog_option_not_equal,
    novel_opcode_09_wait_frames,
    novel_opcode_0a_load_picture,
    novel_opcode_0b_hide_picture,
    novel_opcode_0c_set_parent_windows_visible,
    novel_opcode_0d_start_text_thread,
    novel_opcode_0e_branch_to_random_section,
    novel_opcode_0f_branch_if_section_read,
    novel_opcode_10_branch_if_section_unread,
    novel_opcode_11_branch_if_saved_flag_set,
    novel_opcode_12_branch_if_saved_flag_clear,
    novel_opcode_13_branch_if_local_flag_set,
    novel_opcode_14_branch_if_local_flag_clear,
    novel_opcode_15_set_saved_flag,
    novel_opcode_16_clear_saved_flag,
    novel_opcode_17_set_local_flag,
    novel_opcode_18_clear_local_flag,
    novel_opcode_19_set_counter,
    novel_opcode_1a_add_counter_delta,
    novel_opcode_1b_subtract_counter_delta,
    novel_opcode_1c_show_value_display,
    novel_opcode_1d_hide_value_display,
    novel_opcode_1e_apply_counter_delta,
    novel_opcode_1f_play_sound,
    novel_opcode_20_load_music,
    novel_opcode_21_set_sound_mode,
    novel_opcode_22_branch_if_equal,
    novel_opcode_23_branch_if_not_equal,
    novel_opcode_24_branch_if_less,
    novel_opcode_25_branch_if_greater,
    novel_opcode_26_fade_out_music,
    novel_opcode_27_switch_music_track,
    novel_opcode_28_fade_screen_out,
    novel_opcode_29_fade_screen_in,
    novel_opcode_2a_set_white_screen_overlay,
    novel_opcode_2b_wait_for_circle_press,
    novel_opcode_2c_play_sound_effect_quad,
    novel_opcode_2d_stop_sound_effects,
    novel_opcode_2e_start_countdown_timer,
    novel_opcode_2f_play_background_sound,
    novel_opcode_30_set_master_volume,
    novel_opcode_31_unload_music,
    novel_opcode_32_set_menu_brightness,
    novel_opcode_33_add_counter_delta_if_choice,
    novel_opcode_34_wait_for_fade,
    novel_opcode_35_push_operand_history,
};

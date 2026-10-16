/* Sound novel saved state, script execution, pending sounds, the countdown timer and the sound novel opcodes. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_menu.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void wldcore_opcode_branch_if_dialog_option(s32 branch_if_equal);
static void wldcore_opcode_branch_if_local_flag(s32 expected);
static void wldcore_opcode_branch_if_saved_bit(s32 expected);
static void wldcore_opcode_branch_if_savedata_bit(s32 expected);
static void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record);
static void wldcore_sound_change_scenario_music(s32 music_id);
static s32 wldcore_sound_novel_update_countdown_timer(void);
static void wldcore_sound_play_pending_script_sounds(s32 sound_id, s32 weather_sound_id, s32 track);
static void wldcore_sound_stop_music_and_play_sound(s32 sound_id);
static void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id);

void wldcore_sound_novel_restore_saved_state(s32 render_record) {
    u32* word;
    s32 i;
    s32 picture_id;
    s32 transition;
    s16 sound;
    s32 tint;
    s32 section;
    u16* state_flags;

    if (g_wldcore_saved_record_index < 5) {
        g_wldcore_active_saved_record = g_main_saved_records[g_wldcore_saved_record_index];
    } else {
        wldcore_reset_saved_record_fields(&g_wldcore_active_saved_record);
    }
    g_wldcore_active_saved_record.picture_render_index = render_record;
    g_wldcore_active_saved_record.buffer = &g_main_save_slot_buffer;
    g_wldcore_active_saved_record.state_flags &= ~(WLDCORE_OPCODE_STATE_BRANCH | WLDCORE_OPCODE_STATE_FETCH);
    for (i = 0; i < 1; i++) {
        g_wldcore_sound_novel_local_flags[i] = 0;
    }
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].priority = 0xC;
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        &= ~WORLD_RENDER_RECORD_POP_IN;

    picture_id = g_wldcore_active_saved_record.picture_id;
    if (picture_id != -1) {
        g_wldcore_active_saved_record.picture_id = -1;
        g_wldcore_opcode_state.instruction = (g_wldcore_active_saved_record.picture_x << 24)
            | (g_wldcore_active_saved_record.picture_y << 16) | (picture_id << 8);
        wldcore_opcode_load_picture();
    }
    transition = g_wldcore_active_saved_record.background_set;
    if (transition != -1) {
        g_wldcore_opcode_state.instruction = transition << 8;
        g_wldcore_active_saved_record.background_set = -1;
        g_wldcore_loaded_background_set = -1;
        wldcore_opcode_load_background_set();
    }
    if (g_wldcore_active_saved_record.music_selection & 0xFF) {
        wldcore_sound_enqueue_music_start(g_wldcore_active_saved_record.music_selection);
    } else {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        g_wldcore_active_saved_record.music_selection = 0;
    }
    sound = g_wldcore_active_saved_record.sound_id;
    if (sound != 0) {
        if (sound & 0x4000) {
            wldcore_sound_stop_music_and_play_sound((sound ^ 0x4000));
        } else {
            wldcore_sound_stop_weather_sfx_and_send_quad(sound);
        }
    }
    state_flags = &g_wldcore_active_saved_record.state_flags;
    if (*state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair.first_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .window_pair.second_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (*state_flags & WLDCORE_OPCODE_STATE_VALUE_DISPLAY) {
        g_wldcore_opcode_state.instruction = g_wldcore_active_saved_record.value_display_mode << 8;
        wldcore_opcode_start_pending_value_transition();
    }
    tint = g_wldcore_active_saved_record.brightness;
    if (tint == 0x100) {
        tint = 0xFF;
    }
    g_wldcore_screen_fade_state.boxes[1].r = tint;
    g_wldcore_screen_fade_state.boxes[1].g = tint;
    g_wldcore_screen_fade_state.boxes[1].b = tint;
    g_wldcore_screen_fade_state.priorities[1] = 0xB;
    wldcore_sound_wait_for_queue_drain();
    wldcore_load_sound_novel_files(g_wldcore_saved_record_index);
    file_wait_for_pending_variant();
    section = g_wldcore_active_saved_record.section;
    if (section != 0) {
        /* The 32-bit view preserves register order; the named offset field swaps the index and base. */
        g_wldcore_active_saved_record.script_words
            = &((u32*)&g_main_save_slot_buffer)[((u32*)g_wldcore_active_saved_record.buffer)[section] / sizeof(u32)];
        /* A struct-field store: as a scalar global the store would be
         * scheduled above the table load (GCC 2.7 alias rule). */
        g_wldcore_active_saved_record.word_index = 0;
        *state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    } else {
        *state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    }
}

void wldcore_reset_selected_saved_record(s32 unused) {
    wldcore_reset_saved_record_fields(&g_main_saved_records[g_wldcore_saved_record_index]);
}

/* Reset a sound-novel record to the start of a reading: no text history, no
 * picture or background, no sound, section 0 and brightness 0x30. */
static void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record) {
    record->text_history_3 = -1;
    record->text_history_2 = -1;
    record->text_history_1 = -1;
    record->text_history_0 = -1;
    record->picture_id = -1;
    record->background_set = -1;
    record->sound_mode = 0;
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

/* Find an opcode in a sound-novel script section; return its whole instruction or zero. */
u32 wldcore_sound_novel_find_section_instruction(s32 section, s32 opcode) {
    u32* word;
    s32 i;

    word = &((u32*)&g_main_save_slot_buffer)[g_wldcore_active_saved_record.buffer->offsets[section] / sizeof(u32)];
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

/* Clear a render record's text image, draw text `text_id` centerd
 * horizontally in it (nothing for id 0) and upload it. Also clears the
 * three pending sound script variables. */
void wldcore_window_draw_centered_text(s32 window_index, s32 text_id) {
    point32_t point;
    s16 text_width;
    s16 text_height;
    s32 i;

    text_measure_pixels(&text_width, &text_height, text_find_entry(text_id));
    point.x = (((g_wldcore_window_render_records[window_index].bounds.w - text_width) / 2) & 0xfffe) + 8;
    point.y = 10;
    for (i = 0; i < g_wldcore_window_render_records[window_index].bounds.w
            * g_wldcore_window_render_records[window_index].bounds.h / 8;
        i++) {
        g_wldcore_window_image_buffer[i] = 0;
    }
    if (text_id != 0) {
        window_display_text_entry_variant(window_index, text_id, point, g_wldcore_window_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(window_index, g_wldcore_window_image_buffer);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}

/* Script opcode: play the requested sound, weather quad and music track, then
 * clear the three sound script variables.
 *
 * The switch tests the separately cataloged neighbor
 * g_wldcore_active_saved_record.sound_mode, not part of g_wldcore_active_saved_record.music_selection.
 * The selection byte read goes through *(u8*) so it stays an lbu as in the target. */
static void wldcore_sound_play_pending_script_sounds(s32 sound_id, s32 weather_sound_id, s32 track) {
    s32* music_flags;

    if ((sound_id | weather_sound_id | track) == 0) {
        return;
    }
    switch (g_wldcore_active_saved_record.sound_mode) {
    case 0:
        if (sound_id != MAIN_SFX_NONE) {
            wldcore_sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            wldcore_sound_stop_weather_sfx_and_send_quad(weather_sound_id);
        }
        break;
    case 1:
        if (sound_id != MAIN_SFX_NONE) {
            wldcore_sound_play_effect(sound_id);
        }
        if (weather_sound_id != 0) {
            wldcore_sound_stop_weather_sfx_and_send_quad(weather_sound_id);
        }
        if (track != 0) {
            if ((g_wldcore_active_saved_record.music_selection & 0xFF) != track) {
                wldcore_sound_change_scenario_music(track);
                music_flags = &g_wldcore_audio_queue.music.flags;
                *music_flags |= WLDCORE_AUDIO_MUSIC_PLAYING;
                main_sound_switch_music_track(g_wldcore_active_saved_record.music_selection >> 8, 0, 0);
                main_sound_set_current_music_target(g_wldcore_audio_queue.volume, 0x28);
            }
        }
        break;
    }
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
}

enum {
    WLDCORE_SOUND_QUAD_COMMAND_COUNT = 4,
    WLDCORE_SOUND_QUAD_COMMAND_BASE = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_SOUND_NOVEL, 0),
};

/* Stops the weather sound and plays row sound_id of
 * g_wldcore_sound_quad_command_bytes: four sounds of the sound novel's
 * resource, one on each SFX channel pair. A zero byte selects that
 * resource's empty sound 0, which only stops its channel pair. */
static void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id) {
    s32 values[WLDCORE_SOUND_QUAD_COMMAND_COUNT];
    s32 i;

    main_sound_stop_weather_sfx_music();
    for (i = 0; i < WLDCORE_SOUND_QUAD_COMMAND_COUNT; i++) {
        values[i] = g_wldcore_sound_quad_command_bytes[sound_id][i] + WLDCORE_SOUND_QUAD_COMMAND_BASE;
    }
    suzuki_sound_play_4_sfx(values[0], values[1], values[2], values[3]);
}

/* Stop the weather sound and play environment sound `sound_id` as the
 * background sound. */
static void wldcore_sound_stop_music_and_play_sound(s32 sound_id) {
    main_sound_stop_weather_sfx_music();
    main_sound_play_bg_sfx(sound_id + SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0));
}

/* Same 32-bit value as g_wldcore_sound_novel_countdown_frames; the target stores 0 through
 * both spellings, and a single name lets GCC merge the two stores. */
extern s32 g_wldcore_sound_novel_countdown_frames_reset;

/*
 * Steps the sound novel's choice countdown. Returns 1 on the frame the script
 * is redirected to the timeout section, 0 otherwise.
 *
 * While a countdown is set, each call counts one frame and, with the play
 * time display selected, shows the time remaining (60 frames to the second).
 * When it runs out the message thread is told to close (task 3) and the
 * countdown becomes -1; once that thread has ended the script branches to
 * g_wldcore_sound_novel_countdown_section.
 */
static s32 wldcore_sound_novel_update_countdown_timer(void) {
    s32 elapsed;
    s32 remaining;
    s32 hours;
    s32 days;

    if (g_wldcore_sound_novel_countdown_frames == -1) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return 0;
        }
        g_wldcore_sound_novel_countdown_frames = 0;
        g_wldcore_sound_novel_countdown_frames_reset = 0;
        g_wldcore_active_saved_record.section = g_wldcore_sound_novel_countdown_section;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
        return 1;
    }

    if (g_wldcore_sound_novel_countdown_frames != 0) {
        elapsed = g_wldcore_sound_novel_countdown_elapsed + 1;
        g_wldcore_sound_novel_countdown_elapsed = elapsed;
        remaining = g_wldcore_sound_novel_countdown_frames - elapsed;
        if (g_wldcore_context_value_display_mode == WLDCORE_VALUE_DISPLAY_PLAY_TIME) {
            days = remaining / 216000;
            hours = remaining / 3600;
            g_wldcore_displayed_numeric_value_secondary = hours - days * 60;
            g_wldcore_displayed_numeric_value_tertiary = remaining / 60 - hours * 60;
            g_wldcore_displayed_numeric_value = days % 24;
        }
        if (g_wldcore_sound_novel_countdown_frames == elapsed) {
            world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
            g_wldcore_sound_novel_countdown_frames = -1;
        }
    }
    return 0;
}

void wldcore_opcode_end_script(void) {
    u16* flags;

    flags = &g_wldcore_active_saved_record.state_flags;
    *flags |= WLDCORE_OPCODE_STATE_END;
}

/* Sound-novel opcode: end the section; the script branches to the recorded
 * section after yielding this frame. */
void wldcore_opcode_end_section(void) {
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH | WLDCORE_OPCODE_STATE_YIELD;
}

void wldcore_opcode_nop(void) {
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Start thread 14 on operand_0 | 0x8800 (as
 * wldcore_opcode_start_text_thread does with the opcode operand), then push
 * operand_0 onto the four-entry history
 * g_wldcore_active_saved_record.text_history_0..3. */
void wldcore_opcode_start_text_thread_and_push_history(void) {
    u16 history_1;
    u16 history_2;
    u16 history_3;
    u16 flags;
    s32 operand;

    operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    thread_start(WLDCORE_THREAD_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    history_1 = g_wldcore_active_saved_record.text_history_1;
    history_2 = g_wldcore_active_saved_record.text_history_2;
    history_3 = g_wldcore_active_saved_record.text_history_3;
    flags = g_wldcore_active_saved_record.state_flags;
    g_wldcore_active_saved_record.text_id = operand;
    g_wldcore_active_saved_record.text_history_3 = operand;
    g_wldcore_active_saved_record.text_history_0 = history_1;
    g_wldcore_active_saved_record.text_history_1 = history_2;
    g_wldcore_active_saved_record.text_history_2 = history_3;
    g_wldcore_active_saved_record.state_flags = flags | WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: yield while the message thread is running. */
void wldcore_opcode_wait_for_text_thread(void) {
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_YIELD;
    }
}

/* Sound-novel opcode: load background set operand 0 unless it is the one
 * already recorded, waiting for the file. */
void wldcore_opcode_load_background_set(void) {
    s32 value;

    value = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    if (g_wldcore_active_saved_record.background_set != value) {
        DrawSync(0);
        g_wldcore_active_saved_record.background_set = value;
        wldcore_gfx_load_background_set(value);
        file_wait_for_pending_variant();
    }
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_branch_to_section(void) {
    g_wldcore_active_saved_record.section = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
}

void wldcore_opcode_branch_if_dialog_option_equal(void) {
    wldcore_opcode_branch_if_dialog_option(1);
}

/* Branches the sound-novel script to section operand_0 when the
 * selected dialog option equals operand byte 2 (or differs from it, when `branch_if_equal` is 0); otherwise raises
 * state flag 4. */
static void wldcore_opcode_branch_if_dialog_option(s32 branch_if_equal) {
    u8 operand = (u8)(g_wldcore_active_saved_record.instruction >> 16);

    if (operand == script_get_variable(EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION) ? branch_if_equal == 1
                                                                                : branch_if_equal == 0) {
        g_wldcore_active_saved_record.section = (u8)(g_wldcore_active_saved_record.instruction >> 8);
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    } else {
        u16* flags = &g_wldcore_active_saved_record.state_flags;

        *flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
}

void wldcore_opcode_branch_if_dialog_option_not_equal(void) {
    wldcore_opcode_branch_if_dialog_option(0);
}

void wldcore_opcode_wait_frames(void) {
    u16 flags;
    u16 updated;
    s32 counter;
    s32 operand;

    flags = g_wldcore_active_saved_record.state_flags;
    updated = flags | WLDCORE_OPCODE_STATE_YIELD;
    g_wldcore_active_saved_record.state_flags = updated;
    if ((updated & WLDCORE_OPCODE_STATE_FETCHED) != 0) {
        g_wldcore_sound_novel_wait_counter = 0;
        g_wldcore_sound_novel_wait_frames = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    }
    counter = g_wldcore_sound_novel_wait_counter;
    operand = g_wldcore_sound_novel_wait_frames;
    if (operand < counter) {
        g_wldcore_active_saved_record.state_flags = flags | (WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD);
    } else {
        g_wldcore_sound_novel_wait_counter = counter + 1;
    }
}

/* World-script opcode: loads picture operand_0 through 0x80068ab4, places
 * its render record at (operand_2 - 0x80, operand_1 - 0x78), uploads the
 * TIM's pixels and CLUT, and binds the render record to the VRAM image. */
void wldcore_opcode_load_picture(void) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t unused_slot; /* reserves the target frame's 0x50 slot */
    u32 instruction;
    s32 picture;
    s32 x;
    s32 y;

    instruction = g_wldcore_opcode_state.instruction;
    x = instruction >> 24;
    y = (instruction >> 16) & 0xFF;
    picture = (instruction & 0xFF00) >> 8;
    g_wldcore_opcode_state.picture = picture;
    g_wldcore_opcode_state.x = x;
    g_wldcore_opcode_state.y = y;
    wldcore_proposition_load_picture_data_set(picture);
    file_wait_for_pending_variant();
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base.x = x - 0x80;
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].base.y = y - 0x78;
    GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut_position.x = image.cx;
    clut_position.y = image.cy;
    LoadTPage(image.pixel, 0, 0, bounds.x, bounds.y, bounds.w * 4, bounds.h);
    LoadClut(image.clut, clut_position.x, clut_position.y);
    wldcore_window_init_vram_render_record(
        g_wldcore_opcode_state.render_index, bounds, clut_position, WORLD_RENDER_RECORD_8BIT_CLUT);
    g_wldcore_window_render_records[g_wldcore_opcode_state.render_index].anim_counter = 0;
    g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Hides the sound-novel picture's render record and forgets the picture. */
void wldcore_opcode_hide_picture(void) {
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_active_saved_record.picture_id = -1;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD;
}

/* Script opcode: operand byte 0 sets (non-zero) or clears state flag 0x40
 * and shows or hides both windows of the parent menu level (window flag
 * 0x10), then sets state flag 0x04. */
void wldcore_opcode_set_parent_windows_visible(void) {
    if ((u8)(g_wldcore_opcode_state.instruction >> 8) != 0) {
        g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else {
        g_wldcore_opcode_state.state_flags &= ~WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
    g_wldcore_opcode_state.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_start_text_thread(void) {
    s32 operand;

    operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    thread_start(WLDCORE_THREAD_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 9, operand | TEXT_ID_WORLD_MESSAGE_BASE, 0);
    g_wldcore_active_saved_record.text_id = operand;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: branch to one of the three sections named by the
 * operand bytes, chosen by rand() with equal weight. */
void wldcore_opcode_branch_to_random_section(void) {
    s32 pick;
    s32 value;

    pick = (rand() * 3) >> 15;
    if (pick == 0) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    }
    if (pick == 1) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 16);
    }
    if (pick == 2) {
        value = (u8)(g_wldcore_active_saved_record.instruction >> 24);
    }
    g_wldcore_active_saved_record.section = value;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
}

void wldcore_opcode_branch_if_savedata_bit_set(void) {
    wldcore_opcode_branch_if_savedata_bit(1);
}

/* Script branch on a save-data bit selected by a 16-bit operand: when the
 * bit equals the expected value the branch target byte is armed at 0x800d484e
 * with state flag 0x02, otherwise only flag 0x04 is raised. */
static void wldcore_opcode_branch_if_savedata_bit(s32 expected) {
    u32* instruction = &g_wldcore_opcode_state.instruction;
    u16 flags;

    if (wldcore_test_bit(g_main_saved_data_bits, ((u8)(*instruction >> 8) << 8) | (u8)(*instruction >> 16))
        == expected) {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_BRANCH;
        g_wldcore_active_saved_record.section = (u8)(*instruction >> 24);
    } else {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_FETCH;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}

void wldcore_opcode_branch_if_savedata_bit_clear(void) {
    wldcore_opcode_branch_if_savedata_bit(0);
}

void wldcore_opcode_branch_if_saved_bit_set(void) {
    wldcore_opcode_branch_if_saved_bit(1);
}

/* Script branch on a bit of the second save-data 32-bit array selected by the
 * single operand byte: when the bit equals the expected value the branch
 * target byte is armed at 0x800d484e with state flag 0x02, otherwise only
 * flag 0x04 is raised. */
static void wldcore_opcode_branch_if_saved_bit(s32 expected) {
    u32* instruction = &g_wldcore_opcode_state.instruction;
    u16 flags;

    /* Keep the instruction address live across the call for both operand loads. */
    if (wldcore_test_bit(g_main_secondary_saved_data_bits, (u8)(*instruction >> 8)) == expected) {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_BRANCH;
        g_wldcore_active_saved_record.section = (u8)(*instruction >> 16);
    } else {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_FETCH;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}

void wldcore_opcode_branch_if_saved_bit_clear(void) {
    wldcore_opcode_branch_if_saved_bit(0);
}

void wldcore_opcode_branch_if_local_flag_set(void) {
    wldcore_opcode_branch_if_local_flag(1);
}

/* Script branch on a bit of the world-local bit array at 0x800d4564 selected
 * by the single operand byte: when the bit equals the expected value the
 * branch target byte is armed at 0x800d484e with state flag 0x02, otherwise
 * only flag 0x04 is raised. */
static void wldcore_opcode_branch_if_local_flag(s32 expected) {
    u32* instruction = &g_wldcore_opcode_state.instruction;
    u16 flags;

    if (wldcore_test_bit(g_wldcore_sound_novel_local_flags, (u8)(*instruction >> 8)) == expected) {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_BRANCH;
        g_wldcore_active_saved_record.section = (u8)(*instruction >> 16);
    } else {
        flags = g_wldcore_active_saved_record.state_flags | WLDCORE_OPCODE_STATE_FETCH;
    }
    g_wldcore_active_saved_record.state_flags = flags;
}

void wldcore_opcode_branch_if_local_flag_clear(void) {
    wldcore_opcode_branch_if_local_flag(0);
}

void wldcore_opcode_set_saved_bit(void) {
    wldcore_set_bit_value(g_main_secondary_saved_data_bits, (u8)(g_wldcore_active_saved_record.instruction >> 8), 1);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_clear_saved_bit(void) {
    wldcore_set_bit_value(g_main_secondary_saved_data_bits, (u8)(g_wldcore_active_saved_record.instruction >> 8), 0);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_set_local_flag(void) {
    wldcore_set_bit_value(g_wldcore_sound_novel_local_flags, (u8)(g_wldcore_active_saved_record.instruction >> 8), 1);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_clear_local_flag(void) {
    wldcore_set_bit_value(g_wldcore_sound_novel_local_flags, (u8)(g_wldcore_active_saved_record.instruction >> 8), 0);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_set_counter(void) {
    g_wldcore_active_saved_record.counter = (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: add the 16-bit operand (instruction bits 8..23) to the
 * record's counter delta. */
void wldcore_opcode_add_counter_delta(void) {
    g_wldcore_active_saved_record.counter_delta += (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: subtract the 16-bit operand (instruction bits 8..23)
 * from the record's counter delta. */
void wldcore_opcode_subtract_counter_delta(void) {
    g_wldcore_active_saved_record.counter_delta -= (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: show the context value display in the mode of operand
 * 0 at (-0x20, 0x54). Any mode but the play time shows the record's counter. */
void wldcore_opcode_start_pending_value_transition(void) {
    s32 mode;

    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    mode = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    g_wldcore_active_saved_record.value_display_mode = mode;
    g_wldcore_context_value_display_mode = mode;
    if (mode != WLDCORE_VALUE_DISPLAY_PLAY_TIME) {
        g_wldcore_displayed_numeric_value = g_wldcore_active_saved_record.counter;
    }
    g_wldcore_context_value_display_position.x = -0x20;
    g_wldcore_context_value_display_position.y = 0x54;
    g_wldcore_hud_ot_priority = 0xa;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_VALUE_DISPLAY;
}

/* Opcode 29: take the numeric value display down again (wldcore_opcode_start_pending_value_transition
 * puts it up) and fetch the next instruction. */
void wldcore_opcode_hide_value_display(void) {
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_active_saved_record.state_flags
        = (g_wldcore_active_saved_record.state_flags & ~WLDCORE_OPCODE_STATE_VALUE_DISPLAY)
        | WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_apply_pending_delta_and_clamp_value(void) {
    s32 delta;
    s32* value = &g_wldcore_active_saved_record.counter;

    delta = g_wldcore_active_saved_record.counter_delta;
    g_wldcore_active_saved_record.counter_delta = 0;
    *value += delta;
    if (*value > 0xFFFF) {
        *value = 0xFFFF;
    }
    if (*value < 0) {
        *value = 0;
    }
    g_wldcore_displayed_numeric_value = *value;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_play_sound(void) {
    main_sound_play_sfx_find_channel((u8)(g_wldcore_active_saved_record.instruction >> 8));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_play_music(void) {
    wldcore_sound_change_scenario_music((u8)(g_wldcore_active_saved_record.instruction >> 8));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Replace the sound novel's music with `music_id`: stop the playing track,
 * unload its slot (slot 2 is used when none is loaded) and start the new
 * track; the load-wait loop after it polls a stub that always returns 0. The saved record keeps the slot in the high
 * byte and the music id in the low byte. */
static void wldcore_sound_change_scenario_music(s32 music_id) {
    s16 packed = g_wldcore_active_saved_record.music_selection;
    s32 slot = packed >> 8;

    if ((packed & 0xFF) != 0) {
        main_sound_stop_current_music();
    }
    if (slot != 0) {
        main_sound_unload_scenario_mus(slot);
    } else {
        slot = 2;
    }
    VSync(2);
    main_sound_open_and_play_music(music_id, slot);
    g_wldcore_active_saved_record.music_selection = (slot << 8) | music_id;
    while (main_sound_is_music_loading_stub() != 0) {
        VSync(0);
    }
    g_wldcore_audio_queue.music.tracks[slot - 1] = music_id;
    g_wldcore_audio_queue.current_slot = slot;
}

void wldcore_opcode_set_sound_mode(void) {
    g_wldcore_active_saved_record.sound_mode = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* The double negation keeps the native counter/delta add operand order. */
void wldcore_opcode_branch_if_equal(void) {
    s32 value;
    s32 operand;
    u32 word;

    value = g_wldcore_active_saved_record.counter - (-g_wldcore_active_saved_record.counter_delta);
    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_wldcore_opcode_state.instruction;
    operand = (word >> 8) & 0xffff;
    if (value == operand) {
        g_wldcore_active_saved_record.section = word >> 24;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
}

/* The double negation keeps the native counter/delta add operand order. */
void wldcore_opcode_branch_if_not_equal(void) {
    s32 value;
    s32 operand;
    u32 word;

    value = g_wldcore_active_saved_record.counter - (-g_wldcore_active_saved_record.counter_delta);
    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_wldcore_opcode_state.instruction;
    operand = (word >> 8) & 0xffff;
    if (value != operand) {
        g_wldcore_active_saved_record.section = word >> 24;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
}

/* The double negation keeps the native counter/delta add operand order. */
void wldcore_opcode_branch_if_less(void) {
    s32 value;
    s32 operand;
    u32 word;

    value = g_wldcore_active_saved_record.counter - (-g_wldcore_active_saved_record.counter_delta);
    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_wldcore_opcode_state.instruction;
    operand = (word >> 8) & 0xffff;
    if (value < operand) {
        g_wldcore_active_saved_record.section = word >> 24;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
}

/* The double negation keeps the native counter/delta add operand order. */
void wldcore_opcode_branch_if_greater(void) {
    s32 value;
    s32 operand;
    u32 word;

    value = g_wldcore_active_saved_record.counter - (-g_wldcore_active_saved_record.counter_delta);
    if (0xffff < value) {
        value = 0xffff;
    }
    if (value < 0) {
        value = 0;
    }
    word = g_wldcore_opcode_state.instruction;
    operand = (word >> 8) & 0xffff;
    if (operand < value) {
        g_wldcore_active_saved_record.section = word >> 24;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_BRANCH;
    } else {
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
}

/* Sound-novel opcode: fade the music to silence over operand 0 * 3 steps and
 * clear the playing flag and the selected music id (the slot is kept). */
void wldcore_opcode_fade_out_music(void) {
    s32* state;
    s32 operand;

    state = &g_wldcore_audio_queue.music.flags;
    *state &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
    operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    main_sound_set_current_music_target(0, operand * 3);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    g_wldcore_active_saved_record.music_selection &= 0xFF00;
}

/* Sound-novel opcode: when a track is selected, switch playback to its slot
 * at volume 0 and fade to the queue's volume over operand 0 * 3 steps. */
void wldcore_opcode_switch_music_track(void) {
    s16 track = g_wldcore_active_saved_record.music_selection;
    s32* state;
    s32 operand;

    if (track != 0) {
        state = &g_wldcore_audio_queue.music.flags;
        *state |= WLDCORE_AUDIO_MUSIC_PLAYING;
        operand = (u8)(g_wldcore_active_saved_record.instruction >> 8);
        main_sound_switch_music_track(track >> 8, 0, 0);
        main_sound_set_current_music_target(g_wldcore_audio_queue.volume, operand * 3);
    }
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: fade the screen out over operand 0 frames. The
 * fade-out bit is cleared first so that wldcore_fade_start_screen always
 * starts the fade; the box-visible bit set here is replaced by it. */
void wldcore_opcode_fade_screen_out(void) {
    g_wldcore_screen_fade_state.flags[0]
        = (g_wldcore_screen_fade_state.flags[0] & ~WLDCORE_SCREEN_FADE_OUT) | WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, (u8)(g_wldcore_active_saved_record.instruction >> 8));
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: fade the screen in over operand 0 frames and drop the
 * white-overlay state. */
void wldcore_opcode_fade_screen_in(void) {
    u32* state = &g_wldcore_screen_fade_state.flags[0];
    u16* flags = &g_wldcore_active_saved_record.state_flags;

    *state |= WLDCORE_SCREEN_FADE_OUT;
    *flags &= ~WLDCORE_OPCODE_STATE_WHITE_OVERLAY;
    wldcore_fade_start_screen(0, (u8)(g_wldcore_active_saved_record.instruction >> 8));
    *flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Opcode handler 42 of the table at 0x8009ecfc, immediately after
 * wldcore_opcode_fade_screen_in. Drains the GPU and waits a frame, then either
 * arms the full-screen white overlay (sortbox color 0xff/0xff/0xff, state bit
 * 3 set, transfer bits cleared, state flag 0x100 set, menus blacked out) or
 * tears it down again (color cleared, state bits 0/1/3 cleared, flag 0x100
 * cleared, menus back to neutral 0x80 brightness). The operand byte selects:
 * zero arms it, non-zero clears it. boxes[0].r/green/blue are the cataloged
 * fields at g_wldcore_screen_fade_state+0x24..0x26, gating the draw in
 * wldcore_fade_draw_screen_overlays.
 *
 * The world_menu_set_brightness call and the 0xc store are written once per
 * arm; jump.c cross-jumps the common tail from the g_wldcore_active_saved_record.state_flags store
 * onward, leaving only the three argument constants in the arms.
 */
void wldcore_opcode_set_white_screen_overlay(void) {
    u16* flags;

    DrawSync(0);
    VSync(0);
    if ((u8)(g_wldcore_active_saved_record.instruction >> 8) == 0) {
        u32* state = &g_wldcore_screen_fade_state.flags[0];

        g_wldcore_screen_fade_state.boxes[0].r = 0xff;
        g_wldcore_screen_fade_state.boxes[0].g = 0xff;
        g_wldcore_screen_fade_state.boxes[0].b = 0xff;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
        *state = (*state & ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_OUT)) | WLDCORE_SCREEN_FADE_BOX_VISIBLE;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_WHITE_OVERLAY;
        world_menu_set_brightness(0, 0, 0);
    } else {
        u32* state = &g_wldcore_screen_fade_state.flags[0];

        g_wldcore_screen_fade_state.boxes[0].r = 0;
        g_wldcore_screen_fade_state.boxes[0].g = 0;
        g_wldcore_screen_fade_state.boxes[0].b = 0;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
        *state &= ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_OUT | WLDCORE_SCREEN_FADE_BOX_VISIBLE);
        g_wldcore_active_saved_record.state_flags &= ~WLDCORE_OPCODE_STATE_WHITE_OVERLAY;
        world_menu_set_brightness(0x80, 0x80, 0x80);
    }
    flags = &g_wldcore_active_saved_record.state_flags;
    *flags = *flags | (WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD);
}

/* Each block takes its own pointer local so the flag address is materialized
 * twice, as in the target. */
void wldcore_opcode_wait_for_circle_press(void) {
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        u16* state_flags;

        state_flags = &g_wldcore_active_saved_record.state_flags;
        *state_flags |= WLDCORE_OPCODE_STATE_FETCH;
    }
    {
        u16* state_flags;

        state_flags = &g_wldcore_active_saved_record.state_flags;
        *state_flags |= WLDCORE_OPCODE_STATE_YIELD;
    }
}

/* Sound-novel opcode: stop the weather sound and play sound operand 0. A
 * nonzero operand 1 also records the sound so a restored reading replays
 * it. */
/* Target 0x8008c7ac. */
void wldcore_opcode_play_sound_effect_quad(void) {
    char unused[8];
    s32 sound_id;

    sound_id = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    if (g_wldcore_opcode_state.instruction & 0xFF0000) {
        g_wldcore_active_saved_record.sound_id = sound_id;
    }
    wldcore_sound_stop_weather_sfx_and_send_quad(sound_id);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: stop the weather sound and clear the recorded sound. */
void wldcore_opcode_stop_music(void) {
    main_sound_stop_weather_sfx_music();
    g_wldcore_active_saved_record.sound_id = 0;
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: start a countdown of the 16-bit operand in seconds
 * (60 frames each); when it runs out the script branches to the section in
 * the top operand byte. */
void wldcore_opcode_start_countdown_timer(void) {
    u32 instruction;

    instruction = g_wldcore_opcode_state.instruction;
    g_wldcore_sound_novel_countdown_elapsed = 0;
    g_wldcore_sound_novel_countdown_frames = ((instruction << 8) >> 16) * 60;
    g_wldcore_sound_novel_countdown_section = instruction >> 24;
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: stop the weather sound and play environment sound
 * operand 0 as the background sound. A nonzero operand 1 also records the
 * sound, flagged 0x4000, so a restored reading replays it. */
void wldcore_opcode_stop_music_and_play_sound(void) {
    char unused[8];
    s32 sound_id;

    sound_id = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    if (g_wldcore_opcode_state.instruction & 0xFF0000) {
        g_wldcore_active_saved_record.sound_id = sound_id | 0x4000;
    }
    wldcore_sound_stop_music_and_play_sound(sound_id);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: set the master volume to operand 0 * 0x40 (0xff maps
 * to the maximum 0x3fff) over operand 1 * 3 steps, at least one. */
void wldcore_opcode_change_volume_balance(void) {
    s32 unused[2];
    s32 balance = (u8)(g_wldcore_active_saved_record.instruction >> 16) * 3;
    s32 volume = (g_wldcore_opcode_state.instruction & 0xFF00) >> 2;

    if (balance == 0) {
        balance = 1;
    }
    if (volume == 0x3FC0) {
        volume = 0x3FFF;
    }
    suzuki_sound_set_master_volume(volume, balance);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Script opcode: stop the pending music track, clear its state slot, and mark
 * the opcode complete. */
void wldcore_opcode_stop_pending_music(void) {
    s16* music = &g_wldcore_active_saved_record.music_selection;
    wldcore_audio_music_slots_t* state;
    s32* tracks;

    if (*music != 0) {
        state = &g_wldcore_audio_queue.music;
        state->flags &= ~WLDCORE_AUDIO_MUSIC_PLAYING;
        main_sound_stop_current_music();
        main_sound_unload_scenario_mus(*music >> 8);
        /* Indexed one 32-bit value above the table wldcore_sound_change_scenario_music fills. */
        tracks = state->tracks;
        tracks[*music >> 8] = 0;
        g_wldcore_audio_queue.current_slot = 0;
        *music = 0;
    }
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

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
 * wldcore_opcode_start_text_thread_and_push_history). */
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

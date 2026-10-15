/* Sound novel state restore, interpreter loop, sounds and countdown. */

#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Loads the sound novel's reading state and replays what it implies.
 *
 * Slots 0..4 of g_main_saved_records hold a saved position; any other index
 * starts from a reset record. `render_record` becomes the picture's render
 * record. The saved picture and background set are reloaded through their
 * opcode handlers, the saved music is restarted (or the music faded out and
 * unloaded when none is saved), the saved sound replayed (bit 0x4000 selects
 * wldcore_sound_stop_music_and_play_sound), the level's two button prompts
 * and the value display re-shown when their state flags say so, and the fade
 * box set to the saved
 * brightness (0x100 is stored as 0xFF). After the novel's files are loaded
 * the script resumes at the saved section's 32-bit table entry, or branches to
 * the start when the section is 0.
 */
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
void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record) {
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
void wldcore_sound_play_pending_script_sounds(s32 sound_id, s32 weather_sound_id, s32 track) {
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
void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id) {
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
void wldcore_sound_stop_music_and_play_sound(s32 sound_id) {
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
s32 wldcore_sound_novel_update_countdown_timer(void) {
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

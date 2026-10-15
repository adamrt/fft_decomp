#include "fft/script.h"
#include "fft/wldcore.h"

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

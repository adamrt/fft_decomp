#include "fft/wldcore.h"

/* Provisional view of the picture state at 0x800d485c (render record index,
 * then the pending picture id). */
typedef struct wldcore_script_picture_state {
    s16 render_index; /* 0x00; g_wldcore_active_saved_record.picture_render_index */
    s16 picture;      /* 0x02; g_wldcore_active_saved_record.picture_id */
} wldcore_script_picture_state_t;

void wldcore_sound_novel_restore_saved_state(s32 render_record) {
    u32* word;
    s32 i;
    s32 picture_id;
    s32 transition;
    s16 sound;
    s32 tint;
    s32 section;
    u16* state_flags;
    wldcore_script_picture_state_t* picture;

    if (g_wldcore_saved_record_index < 5) {
        g_wldcore_active_saved_record = g_main_saved_records[g_wldcore_saved_record_index];
    } else {
        wldcore_reset_saved_record_fields(&g_wldcore_active_saved_record);
    }
    g_wldcore_active_saved_record.picture_render_index = render_record;
    g_wldcore_active_saved_record.buffer = g_main_save_slot_buffer;
    g_wldcore_active_saved_record.state_flags &= 0xFFF9;
    for (i = 0; i < 1; i++) {
        g_wldcore_sound_novel_local_flags[i] = 0;
    }
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
    picture = (wldcore_script_picture_state_t*)&g_wldcore_active_saved_record.picture_render_index;
    g_wldcore_window_render_records[picture->render_index].priority = 0xC;
    g_wldcore_window_render_records[picture->render_index].flags |= 0x10;
    g_wldcore_window_render_records[picture->render_index].flags &= ~0x100;

    picture_id = picture->picture;
    if (picture_id != -1) {
        g_wldcore_active_saved_record.picture_id = -1;
        g_wldcore_opcode_state.instruction.word = (g_wldcore_active_saved_record.picture_x << 24)
            | (g_wldcore_active_saved_record.picture_y << 16) | (picture_id << 8);
        wldcore_opcode_load_picture();
    }
    transition = g_wldcore_active_saved_record.background_set;
    if (transition != -1) {
        g_wldcore_opcode_state.instruction.word = transition << 8;
        g_wldcore_active_saved_record.background_set = -1;
        g_wldcore_loaded_background_set = -1;
        wldcore_opcode_load_background_set();
    }
    if (g_wldcore_active_saved_record.music_selection & 0xFF) {
        wldcore_sound_enqueue_music_start((s16)g_wldcore_active_saved_record.music_selection);
    } else {
        wldcore_sound_enqueue_audio_command(2, 0x10);
        wldcore_sound_enqueue_audio_command(4, 2);
        g_wldcore_active_saved_record.music_selection = 0;
    }
    sound = g_wldcore_active_saved_record.sound_id;
    if (sound != 0) {
        if (sound & 0x4000) {
            wldcore_sound_stop_music_and_play_sound((s16)(sound ^ 0x4000));
        } else {
            wldcore_sound_stop_weather_sfx_and_send_quad(sound);
        }
    }
    state_flags = &g_wldcore_active_saved_record.state_flags;
    if (*state_flags & 0x40) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair.first_window]
            .flags &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .window_pair.second_window]
            .flags &= ~0x10;
    }
    if (*state_flags & 0x80) {
        g_wldcore_opcode_state.instruction.word = (s16)g_wldcore_active_saved_record.value_display_mode << 8;
        wldcore_opcode_start_pending_value_transition();
    }
    tint = g_wldcore_active_saved_record.brightness;
    if (tint == 0x100) {
        tint = 0xFF;
    }
    g_wldcore_screen_fade_state.boxes[1].r = tint;
    g_wldcore_screen_fade_state.boxes[1].g = tint;
    g_wldcore_screen_fade_state.boxes[1].b = tint;
    g_wldcore_screen_fade_box_1_priority = 0xB;
    wldcore_sound_wait_for_queue_drain();
    wldcore_load_sound_novel_files(g_wldcore_saved_record_index);
    wldcore_wait_for_file_load();
    section = g_wldcore_active_saved_record.section;
    if (section != 0) {
        g_wldcore_active_saved_record.script_words
            = (u32*)&g_main_save_slot_buffer[(((u32*)g_wldcore_active_saved_record.buffer)[section] >> 2) * 4];
        /* A struct-field store: as a scalar global the store would be
         * scheduled above the table load (GCC 2.7 alias rule). */
        g_wldcore_active_saved_record.word_index = 0;
        *state_flags |= 4;
    } else {
        *state_flags |= 2;
    }
}

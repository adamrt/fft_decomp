/* Pending delta publish, sound/music play and switch, compare branches, screen fades, white overlay, countdown and
 * volume. */

#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

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
void wldcore_sound_change_scenario_music(s32 music_id) {
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

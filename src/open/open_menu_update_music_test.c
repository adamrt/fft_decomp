#include "fft/open.h"
#include "psx/pad.h"

/* Controller 7 (hidden Music Test menu) state, pushed by
 * open_menu_start_music_test_controller. */
typedef struct open_menu_sound_test_state {
    /* 0x00 */ s32 music_id;     /* scenario music selected for playback */
    /* 0x04 */ s32 delay;        /* frames before loading music_id */
    /* 0x08 */ s32 music_slot;   /* slot from main_sound_open_music_into_free_slot */
    /* 0x0c */ s32 step;         /* 0 idle, 1 exiting, 2 fading out, 3 starting track, 4 opening */
    /* 0x10 */ s32 blink_frames; /* formation-mask blink after a selection */
} open_menu_sound_test_state_t;

/* Runs the hidden Music Test menu.
 *
 * Circle fades out any current track and queues the selected entry's music.
 * Cross stops playback or, when idle, closes the menu, reloads OPENBK and
 * returns to the previous controller. Select opens the thread-2 text window
 * on text id 0xC800 + entry; the Music Test track names and comments are
 * stored in OPEN.LZW, but which of them that id selects is unverified. The
 * formation entry mask blinks for ten frames after each selection.
 */
void open_menu_update_music_test(open_menu_sound_test_state_t* state) {
    open_point32_t extent;
    open_point32_t origin;
    s32 buttons;

    if (state->blink_frames != 0 && ++state->blink_frames >= 10) {
        state->blink_frames = 0;
        world_formation_set_display_mode(0);
        g_open_menu_formation_entry_mask ^= 0x5000;
    }

    if (state->step == 4) {
        if (!(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
            extent.x = 0x15;
            extent.y = 0xB;
            origin.x = 0x8C;
            origin.y = 0x14;
            open_menu_start_music_test_list_thread(0xC, 0x60, &extent, &origin);
            state->step = 0;
        }
    } else if (state->step == 3) {
        if (main_return_zero_80043708() == 0) {
            g_open_music_test_sound_handle = main_sound_switch_music_track(state->music_slot, 0x7F, 0x10);
            state->step = 0;
        }
    } else if (state->step == 2) {
        if (!(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
            g_open_system_runtime_flags ^= 0x410;
            open_file_build_openbk_header(0, g_open_file_destination);
            open_file_wait_for_pending();
            g_open_current_controller_index--;
            open_title_push_menu_controller(0);
        }
    } else if (state->step == 1) {
        if (world_thread_is_running(0xC) == 0) {
            state->step = 2;
            open_gfx_start_overlay_fade_in(0x20);
        }
    } else if (state->delay != 0) {
        if (--state->delay == 0) {
            if (state->music_id != 0) {
                if (state->music_slot != 0) {
                    main_sound_unload_scenario_mus(state->music_slot);
                }
                state->music_slot = main_sound_open_music_into_free_slot(state->music_id);
                state->step = 3;
            } else {
                if (state->music_slot != 0) {
                    main_sound_unload_scenario_mus(state->music_slot);
                }
                state->music_slot = 0;
            }
        }
    } else {
        if (state->music_slot != 0 && SuzukiGetMusicPlaying((suzuki_music_t*)g_open_music_test_sound_handle) == 0) {
            main_sound_unload_scenario_mus(state->music_slot);
            state->music_slot = 0;
            state->music_id = 0;
        }
        buttons = g_open_input_new_button_presses;
        if (buttons & PSX_PAD_CIRCLE) {
            world_formation_set_display_mode(1);
            state->blink_frames = 1;
            g_open_menu_formation_entry_mask |= 0x5000;
            if (state->music_id != 0) {
                state->delay = 0x20;
                main_sound_set_current_music_target(0, 0x78);
            } else {
                state->delay = 1;
            }
            state->music_id = g_open_music_test_music_ids[g_open_music_test_list_params.selected_index];
        } else if (buttons & PSX_PAD_CROSS) {
            if (state->music_id != 0) {
                main_sound_set_current_music_target(0, 0x78);
                state->music_id = 0;
                state->delay = 0x20;
            } else {
                state->step = 1;
                world_thread_set_parameters(0xC, 0, 0, 1);
                g_open_system_runtime_flags |= 0x10;
            }
        } else if (buttons & PSX_PAD_SELECT) {
            open_controller_start_text_message(g_open_music_test_list_params.selected_index + 0xC800, 0);
        }
    }
}

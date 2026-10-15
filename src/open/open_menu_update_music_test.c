#include "fft/open.h"
#include "fft/thread.h"
#include "psx/pad.h"

/* Runs the hidden Music Test menu.
 *
 * Circle fades out any current track and queues the selected entry's music.
 * Cross stops playback or, when idle, closes the menu, reloads OPENBK and
 * returns to the previous controller. Select opens the thread-2 text window
 * on text id 0xC800 + entry: the list shows text 0xC000 + entry, section 24
 * of OPEN.LZW (the 96 track titles), and Select the same entry of section 25
 * (the composers' comments); both are still the Japanese text in this
 * release. Up and Down are withheld from the list for ten frames after each
 * selection.
 */
void open_menu_update_music_test(open_controller_record_t* record) {
    open_menu_sound_test_state_t* state = &record->music_test;
    point32_t extent;
    point32_t origin;
    s32 buttons;

    if (state->blink_frames != 0 && ++state->blink_frames >= 10) {
        state->blink_frames = 0;
        world_formation_set_display_mode(0);
        g_open_menu_blocked_buttons ^= PSX_PAD_UP | PSX_PAD_DOWN;
    }

    if (state->step == OPEN_MUSIC_TEST_STEP_OPEN) {
        if (!(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
            extent.x = 0x15;
            extent.y = 0xB;
            origin.x = 0x8C;
            origin.y = 0x14;
            open_menu_start_music_test_list_thread(OPEN_THREAD_SLOT_MUSIC_TEST_LIST, 0x60, &extent, &origin);
            state->step = OPEN_MUSIC_TEST_STEP_IDLE;
        }
    } else if (state->step == OPEN_MUSIC_TEST_STEP_START_TRACK) {
        if (main_sound_is_music_loading_stub() == 0) {
            g_open_music_test_sound_handle = main_sound_switch_music_track(state->music_slot, 0x7F, 0x10);
            state->step = OPEN_MUSIC_TEST_STEP_IDLE;
        }
    } else if (state->step == OPEN_MUSIC_TEST_STEP_FADE_OUT) {
        if (!(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
            g_open_system_runtime_flags
                ^= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED | OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME;
            open_file_build_openbk_header(0, g_open_file_destination);
            file_wait_for_pending_variant();
            g_open_current_controller_index--;
            open_title_push_menu_controller(0);
        }
    } else if (state->step == OPEN_MUSIC_TEST_STEP_CLOSE) {
        if (thread_is_running(OPEN_THREAD_SLOT_MUSIC_TEST_LIST) == 0) {
            state->step = OPEN_MUSIC_TEST_STEP_FADE_OUT;
            open_gfx_start_overlay_fade_in(0x20);
        }
    } else if (state->delay != 0) {
        if (--state->delay == 0) {
            if (state->music_id != 0) {
                if (state->music_slot != 0) {
                    main_sound_unload_scenario_mus(state->music_slot);
                }
                state->music_slot = main_sound_open_music_into_free_slot(state->music_id);
                state->step = OPEN_MUSIC_TEST_STEP_START_TRACK;
            } else {
                if (state->music_slot != 0) {
                    main_sound_unload_scenario_mus(state->music_slot);
                }
                state->music_slot = 0;
            }
        }
    } else {
        if (state->music_slot != 0 && suzuki_smd_is_music_playing(g_open_music_test_sound_handle) == 0) {
            main_sound_unload_scenario_mus(state->music_slot);
            state->music_slot = 0;
            state->music_id = 0;
        }
        buttons = g_open_input_new_button_presses;
        if (buttons & PSX_PAD_CIRCLE) {
            world_formation_set_display_mode(1);
            state->blink_frames = 1;
            g_open_menu_blocked_buttons |= PSX_PAD_UP | PSX_PAD_DOWN;
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
                state->step = OPEN_MUSIC_TEST_STEP_CLOSE;
                thread_set_parameters(OPEN_THREAD_SLOT_MUSIC_TEST_LIST, 0, 0, 1);
                g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
            }
        } else if (buttons & PSX_PAD_SELECT) {
            open_controller_start_text_message(g_open_music_test_list_params.selected_index + 0xC800, 0);
        }
    }
}

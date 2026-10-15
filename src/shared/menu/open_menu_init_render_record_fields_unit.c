#include "fft/open.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Prepare a 56-byte render record for an image that is already in VRAM.
 *
 * vram_position is the image's VRAM position and image_size its size. The source
 * rectangle keeps x within the texture page of 64 16-bit columns and doubles x and the width
 * (4-bit texels). `clut_position` becomes the record's CLUT position and `flags` replaces
 * the 8-bit, blank and hidden bits. The WLDCORE twin
 * wldcore_window_init_vram_render_record stores a texture page id where this one
 * stores the VRAM position. Nothing in OPEN.BIN calls it. */
void open_menu_init_render_record_fields(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags) {
    RECT* quad_base;
    RECT* quad;
    CVECTOR* color;

    g_open_gfx_render_records_56[record_index].flags
        = (g_open_gfx_render_records_56[record_index].flags
              & ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN))
        | flags;
    g_open_gfx_render_records_56[record_index].tail.clut = clut_position;
    g_open_gfx_render_records_56[record_index].vram_x = vram_position.x;
    g_open_gfx_render_records_56[record_index].vram_y = vram_position.y;
    /* Direct quad indexing changes the original record and column base selection. */
    quad_base = &g_open_gfx_render_records_56[0].tail.quad;
    quad = quad_base + record_index * (sizeof(open_render_record_56_t) / sizeof(RECT));
    quad->x = (vram_position.x & 0x3f) * 2;
    quad->y = vram_position.y & 0xFF;
    quad->w = image_size.x * 2;
    quad->h = image_size.y;
    color = &g_open_gfx_render_records_56[record_index].color;
    color->b = color->g = color->r = 0x80;
    g_open_gfx_render_records_56[record_index].pop_in_step = 0;
}

/* Fills the music-test list's column layout and menu entry, then starts the
 * AT list thread on them. The window is the extent's width plus a 24-pixel
 * margin, rounded down to a multiple of four; its height fields start at 0. */
void open_menu_start_music_test_list_thread(s32 thread_id, s32 limit, point32_t* extent, point32_t* origin) {
    s32 width;

    width = (extent->x + 0x18) & 0xFFFC;
    g_open_music_test_list_state.row_count = extent->y;
    g_open_music_test_list_state.hidden_rows = limit - extent->y;
    g_open_music_test_list_state.x[1] = 4;
    g_open_music_test_list_state.x[0] = 4;
    g_open_music_test_list_state.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
    g_open_music_test_list_state.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
    g_open_music_test_list_state.text_ids[0] = g_open_music_test_list_entries;
    g_open_music_test_list_state.text_colors[0] = g_open_music_test_list_entry_flags;
    g_open_music_test_list_state.mode[0] = MENU_TEXT_COLUMN_TEXT;
    g_open_music_test_list_params.vram_origin.x = 0x200;
    g_open_music_test_list_params.vram_origin.y = 0x100;
    g_open_music_test_list_params.inner_width = width;
    g_open_music_test_list_params.inner_height = 0;
    g_open_music_test_list_params.window.x = origin->x - 0x80;
    g_open_music_test_list_params.window.y = origin->y - 0x78;
    g_open_music_test_list_params.window.w = width;
    g_open_music_test_list_params.window.h = 0;
    g_open_music_test_list_params.text_image.x_load = 0;
    g_open_music_test_list_params.text_image.y_load = 0;
    g_open_music_test_list_params.text_image.width = width;
    g_open_music_test_list_params.text_image.height = 0;
    g_open_music_test_list_params.text_image.x_screen_offset = 0;
    g_open_music_test_list_params.text_image.y_screen_offset = 0;
    g_open_music_test_list_params.text_id = 0;
    g_open_music_test_list_params.max_row_index = extent->y;
    g_open_music_test_list_params.thread_count = 0;
    g_open_music_test_list_params.row_actions = g_open_music_test_row_actions;
    g_open_music_test_list_params.header_id = 1;
    g_open_music_test_list_params.context = &g_open_music_test_list_state;
    g_open_music_test_list_params.value = &g_open_music_test_selected_option;
    g_open_music_test_list_params.selected_index = 0;
    g_open_music_test_list_params.select_text_table = 0;

    thread_start(thread_id, world_screen_build_at_list);
    thread_set_parameters(thread_id, (s32)&g_open_music_test_list_params, 0, 0);
}

/* Open the music test: read OPNBK image 1 as the background, fill the 96-entry
 * track list (text ids 0xc000 + entry, flags clear) and push the music test
 * controller (handler 7) at OPEN_MUSIC_TEST_STEP_OPEN as the screen is revealed.
 *
 * The list is drawn by the WORLD menu frame, which this turns on with Circle,
 * Cross and Select withheld from it and ordering-table entry 4 as its base. The
 * only caller is step 5 of open_title_step_new_game_start. */
void open_menu_start_music_test_controller(void) {
    s32 i;
    s32 four;
    s32 controller;

    open_file_build_openbk_header(1, g_open_file_destination);
    file_wait_for_pending_variant();

    for (i = 0; i < 96; i++) {
        g_open_music_test_list_entries[i] = i + 0xc000;
        g_open_music_test_list_entry_flags[i] = 0;
    }

    script_set_event_speed_variant(1);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    /* The same 4 is stored twice, as the formation OT index and as OPEN_MUSIC_TEST_STEP_OPEN; the local
     * keeps it in a saved register across the fade-out call instead of being rematerialized. */
    four = 4;
    g_open_menu_formation_otag_index = four;
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME;
    open_gfx_start_overlay_fade_out(0x20);

    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].music_test.music_id = 0;
    g_open_controller_records_next[controller].music_test.delay = 0;
    g_open_controller_records_next[controller].music_test.music_slot = 0;
    g_open_controller_records_next[controller].music_test.step = four;
    g_open_controller_records_next[controller].music_test.blink_frames = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_MUSIC_TEST;
    g_open_current_controller_index = controller + 1;
}

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
        g_menu_blocked_buttons ^= PSX_PAD_UP | PSX_PAD_DOWN;
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
            g_menu_blocked_buttons |= PSX_PAD_UP | PSX_PAD_DOWN;
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

/* Start the text message thread and push the controller that waits for it. */
void open_controller_start_text_message(s32 parameter, s32 run_followup) {
    {
        s32 controller = g_open_current_controller_index;

        g_open_controller_records_next[controller].text_message.run_followup = run_followup;
    }
    if (run_followup != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_open_text_section_offsets);
    }

    thread_start(THREAD_SCREEN_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, parameter, 0);

    {
        s32 controller = g_open_current_controller_index;
        s32 saved_blocked_buttons = g_menu_blocked_buttons;

        g_menu_blocked_buttons = -1;
        g_open_controller_records_next[controller].text_message.saved_blocked_buttons = saved_blocked_buttons;
    }
    main_sound_play_sfx(MAIN_SFX_WINDOW_OPEN);

    {
        s32 controller = g_open_current_controller_index;
        s32 next_controller = controller + 1;

        g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TEXT_MESSAGE;
        g_open_current_controller_index = next_controller;
    }
}

/* Handler 8: once the message thread has ended, give back the blocked buttons,
 * pop the controller and, with run_followup, restore the text section pointers
 * open_controller_start_text_message replaced. */
void open_controller_wait_text_message(open_controller_record_t* record) {
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
        return;
    }
    g_menu_blocked_buttons = record->text_message.saved_blocked_buttons;
    g_open_current_controller_index--;
    if (record->text_message.run_followup != 0) {
        text_restore_section_pointers();
    }
}

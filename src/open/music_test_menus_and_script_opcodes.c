/* Music test menus and script opcodes. Its start is a boundary the binary proves; its end is a boundary the binary
 * proves. */

#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

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

void open_movie_stream_title_demo_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level) {
    s32 controller;

    open_movie_start_stream(start_sector, end_frame, -1, cd_mix_level);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].movie.start_sector = start_sector;
    g_open_controller_records_next[controller].movie.stream_length = stream_length;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TITLE_DEMO_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* Handler 11, a title demo movie: Start stops the stream. Once the stream has
 * stopped, by itself or on the update after Start, the display is blanked and the
 * controller is replaced by the FFTST.STR logo movie, which leads back to the
 * title menu. */
void open_movie_start_fftst_or_skip_controller(open_controller_record_t* unused_record) {
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        SetDispMask(0);
        g_active_graphics_buffer_index = 1;
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        g_open_current_controller_index--;
        open_movie_stream_fftst_and_push_controller(DISC_SECTOR_OPEN_FFTST_STR, 0x278a, 0x3df, 0x2de);
    } else if ((g_open_input_new_button_presses & PSX_PAD_START) != 0) {
        open_movie_stop_stream(1);
    }
}

/* New Game on the title menu. With a memory card in either slot the new-game
 * prologue starts at once. With none the frame image (30,720 32-bit values) is saved to
 * g_open_work_buffer_0 and blanked, animation 0x12 is shown on the black screen
 * and the title-exit controller (handler 12) is pushed: Cross returns to the
 * title menu, Circle or Start starts the prologue anyway. */
void open_title_start_new_game_or_clear_file_buffer(void) {
    s32* source;
    s32* destination;
    s32 index;
    s32 record;
    point32_t* position;
    s32 controller;
    s32 _card_status;
    s32 frame_padding[2];

    _card_status = open_card_check_slots();
    index = 0;
    if (_card_status != 0) {
        open_title_start_new_game_transition();
        return;
    }

    source = g_open_file_destination;
    destination = (s32*)g_open_work_buffer_0;
    for (; index < 30720; index++) {
        *destination = *source;
        *source = 0;
        source++;
        destination++;
    }

    open_gfx_start_overlay_fade_out(0x10);
    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_gfx_render_records_36[record].sequence = 0x12;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    position = &g_open_gfx_render_records_36[record].position;
    controller = g_open_current_controller_index;
    position->x = -4;
    position->y = -16;
    g_open_controller_records_next[controller].title_exit.state = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TITLE_EXIT;
    g_open_current_controller_index = controller + 1;
}

/* Per-frame handler of the title-exit controller (handler 12), pushed when New
 * Game is chosen with no memory card inserted.
 *
 * Cross, or Circle or Start, records the choice and fades the screen to black.
 * When the fade has ended the notice record and the controller are released: on
 * Cross the frame image saved in g_open_work_buffer_0 is copied back and the
 * title menu is pushed again; on Circle or Start the new-game prologue starts. */
void open_title_update_exit_transition(open_controller_record_t* record) {
    s32 state;
    u32 buttons;

    if ((g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) != 0) {
        return;
    }

    if (record->title_exit.state != OPEN_TITLE_EXIT_INPUT) {
        g_open_gfx_next_render_record_36 -= 1;
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_current_controller_index -= 1;
        state = record->title_exit.state;
        if (state == OPEN_TITLE_EXIT_CANCEL) {
            s32* source;
            s32* destination;
            s32 index;

            SetDispMask(0);
            source = (s32*)g_open_work_buffer_0;
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            destination = g_open_file_destination;
            for (index = 0; index < 0x7800; index++) {
                destination[index] = source[index];
            }
            open_title_push_menu_controller(0);
        } else if (state == OPEN_TITLE_EXIT_CONFIRM) {
            open_title_start_new_game_transition();
        }
        return;
    }

    buttons = g_open_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        open_gfx_start_overlay_fade_in(0x20);
        record->title_exit.state = OPEN_TITLE_EXIT_CANCEL;
    } else if ((buttons & (PSX_PAD_CIRCLE | PSX_PAD_START)) != 0) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_gfx_start_overlay_fade_in(0x20);
        record->title_exit.state = OPEN_TITLE_EXIT_CONFIRM;
    }
}

void open_gfx_init_transition_request(s32 target, s32 ot_layer, open_gfx_transition_entry_t** entry_table) {
    g_open_gfx_transition.flags = OPEN_GFX_TRANSITION_NEXT_ENTRY;
    g_open_gfx_transition.current_index = 0;
    g_open_gfx_transition.target_index = target;
    g_open_gfx_transition.entry_table = entry_table;
    g_open_gfx_transition.ot_layer = ot_layer;
    g_open_gfx_transition.intensity = 0x80;
}

/* Empty. open_title_step_new_game_start calls it once the transition sequence has finished. */
void open_gfx_transition_finished_stub(void) {
}

/* Advance the new-game prologue by one frame: the picture crossfade and the text
 * of the current transition entry. Returns 1 while it runs and 0 when it has
 * finished.
 *
 * An entry is one page of text lines. NEXT_ENTRY loads the next entry, or
 * finishes at target_index: its line count, texture page and image operation,
 * where 0x10 + n prepares the crossfade to picture n and 0x20 waits for that
 * picture's read and starts the crossfade. NEXT_STEP starts the next line,
 * reading every line's texture row and width from the entry on the first; the
 * timer starts at -16 and the line is revealed one pixel per frame. After the
 * last line the page waits for Circle (WAIT_CIRCLE), dims from 0x80 by 2 per
 * frame (DIM) and the next entry follows.
 *
 * Start sets SKIP: the lines keep advancing while the brightness runs down, and
 * the sequence ends once it is 0 and the crossfade has stopped. */
s32 open_gfx_update_transition_sequence(void) {
    u16 flags;
    s32 i;
    open_gfx_transition_entry_t* entry;
    s32 image_command;

    open_gfx_step_image_crossfade();
    if (!(g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP)) {
        if (g_open_input_new_button_presses & PSX_PAD_START) {
            main_sound_play_sfx(MAIN_SFX_COMPLETE);
            g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_SKIP;
        }
        flags = g_open_gfx_transition.flags;
        if (!(flags & OPEN_GFX_TRANSITION_SKIP)) {
            if (flags & OPEN_GFX_TRANSITION_DIM) {
                g_open_gfx_transition.intensity -= 2;
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                if (g_open_gfx_transition.intensity == 0) {
                    g_open_gfx_transition.intensity = 0x80;
                    g_open_gfx_transition.flags
                        = (g_open_gfx_transition.flags ^ OPEN_GFX_TRANSITION_DIM) | OPEN_GFX_TRANSITION_NEXT_ENTRY;
                }
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_WAIT_CIRCLE) {
                if (g_open_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_WAIT_CIRCLE) | OPEN_GFX_TRANSITION_DIM;
                    g_open_gfx_transition.intensity = 0x80;
                }
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_NEXT_ENTRY) {
                flags ^= OPEN_GFX_TRANSITION_NEXT_ENTRY;
                g_open_gfx_transition.flags = flags;
                if (g_open_gfx_transition.current_index == g_open_gfx_transition.target_index) {
                    return 0;
                }
                g_open_gfx_transition.flags = flags | OPEN_GFX_TRANSITION_NEXT_STEP;
                entry = g_open_gfx_transition.entry_table[g_open_gfx_transition.current_index];
                g_open_gfx_transition.entry = entry;
                g_open_gfx_transition.step_count = (u8)entry->header;
                g_open_gfx_transition.tpage_x = (entry->header & 0xf00) >> 2;
                g_open_gfx_transition.tpage_y = (entry->header >> 4) & 0x100;
                /* The high nibble selects the operation; the low nibble selects its image. */
                image_command = (u8)(entry->header >> 16);
                switch (image_command & OPEN_GFX_TRANSITION_IMAGE_OPERATION_MASK) {
                case OPEN_GFX_TRANSITION_PREPARE_IMAGE:
                    open_gfx_copy_file_image_to_210x180_buffer(image_command & OPEN_GFX_TRANSITION_IMAGE_INDEX_MASK);
                    break;
                case OPEN_GFX_TRANSITION_START_CROSSFADE:
                    file_wait_for_pending_variant();
                    g_open_gfx_image_copy_row_flags[0] = 1;
                    break;
                }
                {
                    s16* index = &g_open_gfx_transition.current_index;

                    g_open_gfx_transition.step = 0;
                    (*index)++;
                }
            }
        }
    }

    flags = g_open_gfx_transition.flags;
    if (flags & OPEN_GFX_TRANSITION_NEXT_STEP) {
        g_open_gfx_transition.flags = flags ^ OPEN_GFX_TRANSITION_NEXT_STEP;
        /* Every line is out: the page waits for Circle. */
        if (g_open_gfx_transition.step == g_open_gfx_transition.step_count) {
            g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_NEXT_STEP)
                | (OPEN_GFX_TRANSITION_NEXT_ENTRY | OPEN_GFX_TRANSITION_WAIT_CIRCLE);
            open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
            return 1;
        }
        /* Byte indexing preserves the retail address-add order and entry reloads. */
        if (g_open_gfx_transition.step == 0) {
            for (i = 0; i < g_open_gfx_transition.step_count; i++) {
                g_open_gfx_transition.line_widths[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, width)];
                g_open_gfx_transition.line_texture_rows[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, texture_row)];
            }
        }
        {
            s16* step = &g_open_gfx_transition.step;

            g_open_gfx_transition.timer = -0x10;
            (*step)++;
        }
    }
    if (++g_open_gfx_transition.timer == g_open_gfx_transition.line_widths[g_open_gfx_transition.step - 1]) {
        g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_NEXT_STEP;
    }
    if (g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP) {
        if (g_open_gfx_transition.intensity != 0) {
            g_open_gfx_transition.intensity -= 2;
        } else if (g_open_gfx_image_copy_row_flags[0] == 0) {
            return 0;
        }
    }
    open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
    return 1;
}

/* Draw the text lines of the current new-game prologue entry at the given
 * brightness.
 *
 * Each step of the entry is one line of text: line_texture_rows holds its 16-pixel row
 * in the texture page and line_widths its width in pixels. The lines are
 * centerd, 20 pixels apart, as a block around the middle of the screen. A
 * finished line is drawn at full width and the line in progress up to timer
 * pixels; the pass with i == step adds that line's leading edge, at most 16
 * pixels whose right-hand corners are black, so the text fades in as it is
 * revealed.
 *
 * Every piece is two POLY_GT4s: the text, added to the picture at intensity in
 * ordering-table entry ot_layer, and a copy one pixel down and right, subtracted at
 * half intensity in entry ot_layer + 1.
 *
 * The coordinates are written in setXY4 order (x0, y0, x1, y1, ...) per
 * primitive, and width + 1 goes through its own variable (the target adds left
 * afterwards). */
void open_gfx_draw_transition_line_strips(s32 intensity) {
    POLY_GT4* prims;
    s32 i;
    s32 prim_index;
    s32 width;
    s32 width_plus_one;
    s32 left;
    s32 top_y;
    s32 u;
    s32 v;
    u16 clut;

    for (i = 0; i < g_open_gfx_transition.step + 1; i++) {
        prim_index = i * 2;
        prims = g_open_gfx_transition_line_primitives[g_active_graphics_buffer_index];
        SetPolyGT4(&prims[prim_index]);
        SetPolyGT4(&prims[prim_index + 1]);

        if (i == g_open_gfx_transition.step) {
            /* Leading edge of the line in progress (line i - 1): black on its right. */
            if (g_open_gfx_transition.line_widths[i - 1] < g_open_gfx_transition.timer) {
                continue;
            }
            width = g_open_gfx_transition.line_widths[i - 1] - g_open_gfx_transition.timer;
            if (g_open_gfx_transition.timer < 0) {
                width = g_open_gfx_transition.timer + 0x10;
            } else if (width >= 0x11) {
                width = 0x10;
            }
            u = g_open_gfx_transition.timer;
            if (u < 0) {
                u = 0;
            }
            v = g_open_gfx_transition.line_texture_rows[i - 1] * 0x10;
            setRGB1(&prims[prim_index], 0, 0, 0);
            setRGB1(&prims[prim_index + 1], 0, 0, 0);
            setRGB3(&prims[prim_index], 0, 0, 0);
            setRGB3(&prims[prim_index + 1], 0, 0, 0);
            left = u - g_open_gfx_transition.line_widths[i - 1] / 2;
            top_y = (i - 1) * 0x14 - g_open_gfx_transition.step_count * 10;
        } else {
            /* Line i: the revealed part of the line in progress, or a whole finished line. */
            if (i == g_open_gfx_transition.step - 1) {
                width = g_open_gfx_transition.timer;
            } else {
                width = g_open_gfx_transition.line_widths[i];
            }
            u = 0;
            if (width <= 0) {
                continue;
            }
            v = g_open_gfx_transition.line_texture_rows[i] * 0x10;
            setRGB1(&prims[prim_index], intensity, intensity, intensity);
            setRGB1(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            setRGB3(&prims[prim_index], intensity, intensity, intensity);
            setRGB3(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
            left = -(g_open_gfx_transition.line_widths[i] / 2);
            top_y = i * 0x14 - g_open_gfx_transition.step_count * 10;
        }

        setRGB0(&prims[prim_index], intensity, intensity, intensity);
        setRGB0(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);
        setRGB2(&prims[prim_index], intensity, intensity, intensity);
        setRGB2(&prims[prim_index + 1], intensity / 2, intensity / 2, intensity / 2);

        clut = GetClut(0, 0x1e3);
        prims[prim_index + 1].clut = clut;
        prims[prim_index].clut = clut;
        prims[prim_index].tpage = GetTPage(0, 1, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);
        prims[prim_index + 1].tpage = GetTPage(0, 2, g_open_gfx_transition.tpage_x, g_open_gfx_transition.tpage_y);

        setXY4(&prims[prim_index], left, top_y, left + width, top_y, left, top_y + 0x10, left + width, top_y + 0x10);
        width_plus_one = width + 1;
        setXY4(&prims[prim_index + 1], left + 1, top_y + 1, left + width_plus_one, top_y + 1, left + 1, top_y + 0x11,
            left + width_plus_one, top_y + 0x11);
        setUV4(&prims[prim_index], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);
        setUV4(&prims[prim_index + 1], u, v, u + width, v, u, v + 0x10, u + width, v + 0x10);

        SetSemiTrans(&prims[prim_index], 1);
        SetSemiTrans(&prims[prim_index + 1], 1);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer], &prims[prim_index]);
        AddPrim(&g_open_gfx_otags[g_active_graphics_buffer_index][g_open_gfx_transition.ot_layer + 1],
            &prims[prim_index + 1]);
    }
}

#define OPEN_IMAGE_COPY_WIDTH  210
#define OPEN_IMAGE_COPY_HEIGHT 180

/* Prepare the crossfade of the new-game prologue's picture to OPNBK image
 * file_index + 2.
 *
 * Points the crossfade at its three buffers, starts the asynchronous read of the
 * new image into g_open_work_buffer_0 (the crossfade's source) and copies the
 * 210x180 picture on screen out of the frame image into g_open_work_buffer_1,
 * packed 210 pixels per row. It clears the per-row fade steps, the running flag
 * (row_flags[0]) and the progress row (row_flags[1], the same byte as
 * g_open_gfx_image_copy_progress); image operation 0x20 of a later transition
 * entry waits for the read and sets the running flag. */
void open_gfx_copy_file_image_to_210x180_buffer(s32 file_index) {
    u16* copy_source;
    u16* copy_destination;
    u16* file_pixels;
    s32 row;
    s32 source_row;
    s32 column;
    s32 source_offset;
    s32 destination_index;
    s32 source_index;
    u16* pixels;
    u16 value;
    u16(*file_image)[256];

    copy_source = (u16*)g_open_work_buffer_0;
    copy_destination = (u16*)g_open_work_buffer_1;
    file_image = g_open_file_destination;
    /* The 210x180 picture begins at row 43 of the 256-pixel frame image. */
    file_pixels = file_image[43];
    g_open_gfx_image_copy_source_buffer = copy_source;
    g_open_gfx_image_copy_destination_buffer = copy_destination;
    g_open_gfx_image_copy_file_pixels = file_pixels;
    open_file_build_openbk_header(file_index + 2, g_open_work_buffer_0);

    row = 0;
    source_row = 0;
    for (; row < OPEN_IMAGE_COPY_HEIGHT; row++) {
        column = 0;
        source_offset = row << 8;
        while (column < OPEN_IMAGE_COPY_WIDTH) {
            destination_index = source_row + column;
            source_index = source_offset + column++;
            destination_index <<= 1;
            pixels = g_open_gfx_image_copy_file_pixels;
            value = pixels[source_index];
            /* Destination indexing hoists pointer loads; byte-pointer arithmetic changes registers. */
            *(u16*)(destination_index + (u32)g_open_gfx_image_copy_destination_buffer) = value;
        }
        g_open_gfx_image_copy_row_flags[row + 2] = 0;
        source_row += OPEN_IMAGE_COPY_WIDTH;
    }

    g_open_gfx_image_copy_row_flags[0] = 0;
    g_open_gfx_image_copy_row_flags[1] = 0;
}

#define OPEN_IMAGE_WIDTH             210
#define OPEN_IMAGE_HEIGHT            180
#define OPEN_IMAGE_ROW_STRIDE        256
#define OPEN_IMAGE_BLEND_BAND_HEIGHT 17
#define OPEN_IMAGE_BLEND_END_ROW     (OPEN_IMAGE_HEIGHT + OPEN_IMAGE_BLEND_BAND_HEIGHT)

/*
 * Advance the bottom-up crossfade between the two 210x180 opening images by one row.
 *
 * Each call blends the 17-row band ending at the progress row: every row in the
 * band steps its own fade counter and writes the per-component mix of the source
 * and destination pixels into the file image.
 *
 * The reverse row is computed inside the row test, which makes it local to that
 * block. GCC 2.7.2's local-alloc orders exactly three block quantities with a
 * fixed-index comparison that would allocate the first half of the *210 multiply
 * first; with four it sorts by priority and the second half gets $v0, as in the
 * target. The s16 components are copied for the compare and the difference,
 * while the sums use the originals.
 */
void open_gfx_step_image_crossfade(void) {
    s32 row;
    s32 last_row;
    s32 reverse_row;
    s32 column;
    s32 source_row_start;
    s32 destination_row_start;
    u16 source_pixel;
    u16 destination_pixel;
    u16 pixel;
    s32 component;
    s16 source_component;
    s16 destination_component;
    s32 previous_components;
    s32 difference;
    u8 fade;

    if (g_open_gfx_image_copy_row_flags[0] != 0) {
        row = g_open_gfx_image_copy_progress - OPEN_IMAGE_BLEND_BAND_HEIGHT;
        if (row < g_open_gfx_image_copy_progress) {
            last_row = OPEN_IMAGE_HEIGHT - 1;
            do {
                if (row >= 0 && row < OPEN_IMAGE_HEIGHT) {
                    reverse_row = last_row - row;
                    column = 0;
                    source_row_start = reverse_row * OPEN_IMAGE_WIDTH;
                    destination_row_start = reverse_row * OPEN_IMAGE_ROW_STRIDE;
                    fade = g_open_gfx_image_copy_row_flags[row + 2]++;
                    do {
                        /* An ordinary source-pointer load is hoisted and adds a saved-register frame. */
                        source_pixel = g_open_gfx_image_copy_source_buffer[source_row_start + column];
                        destination_pixel = g_open_gfx_image_copy_destination_buffer[source_row_start + column];
                        pixel = 0;
                        if (source_pixel != destination_pixel) {
                            for (component = 2; component >= 0; component--) {
                                previous_components = pixel >> 5;
                                source_component = source_pixel & 0x1F;
                                destination_component = destination_pixel & 0x1F;
                                if (source_component != destination_component) {
                                    difference = source_component - destination_component;
                                    pixel = previous_components
                                        | ((destination_component + (s16)((s16)(difference << 8) / 16 * fade) / 256)
                                            << 10);
                                } else {
                                    pixel = previous_components | (source_component << 10);
                                }
                                source_pixel >>= 5;
                                destination_pixel >>= 5;
                            }
                        } else {
                            pixel = source_pixel;
                        }
                        g_open_gfx_image_copy_file_pixels[destination_row_start + column]
                            = pixel | (destination_pixel & 0x8000);
                        column++;
                    } while (column < OPEN_IMAGE_WIDTH);
                }
                row++;
            } while (row < g_open_gfx_image_copy_progress);
        }
        {
            u8* progress = &g_open_gfx_image_copy_progress;

            if (++*progress >= OPEN_IMAGE_BLEND_END_ROW) {
                g_open_gfx_image_copy_row_flags[0] = 0;
            }
        }
    }
}

void open_script_init_state(void) {
    RECT rect;
    s32 i;
    s32 j;
    POLY_FT4* prim;

    SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x200, 0xf0);
    SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x200, 0xf0);
    g_open_gfx_screen_environments[0].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
    g_open_gfx_screen_environments[1].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
    g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
    g_open_gfx_overlay_fade.rect.x = -0x100;
    g_open_gfx_overlay_fade.rect.y = -0x78;
    g_open_gfx_overlay_fade.rect.w = 0x200;
    g_open_gfx_overlay_fade.rect.h = 0xf0;
    g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x200;
    rect.h = 0x1e0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);

    g_open_script_state.dispatch.flags = OPEN_SCRIPT_DISPATCH_RUNNING;
    g_open_script_state.dispatch.tpage = (s16)GetTPage(0, 2, 0x300, 0);
    g_open_script_state.dispatch.clut = (s16)GetClut(0, 0x1e0);
    g_open_script_state._unknown_1674 = 0;
    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = 0;
    g_open_script_state.dispatch.font_index = 0;
    g_open_script_state.dispatch.wide_glyphs = 0;
    g_open_script_state.dispatch.skew = 0;
    g_open_script_state.dispatch.cursor.byte_offset = 0;
    g_open_script_state.dispatch.cursor.data_base = g_open_script_initial_data_base;
    for (j = 0; j < 16; j++) {
        g_open_script_state.records[j].flags = 0;
    }
    g_open_script_state.opntex.fade_intensity = 0;

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_credit_band_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_credit_band_primitives[i].tpage = GetTPage(0, 2, 0x300, 0);
        g_open_gfx_credit_band_primitives[i].clut = GetClut(0x10, 0x1e0);
        setRGB0(prim, 0x80, 0x80, 0x80);
    }

    setUV4(&g_open_gfx_credit_band_primitives[0], 0xf8, 0, 0xf9, 0, 0xf8, 0x30, 0xf9, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[0], -0x100, 0x48, 0x100, 0x48, -0x100, 0x78, 0x100, 0x78);
    setUV4(&g_open_gfx_credit_band_primitives[1], 0xfc, 0, 0xfd, 0, 0xfc, 0x30, 0xfd, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[1], -0x100, -0x78, 0x100, -0x78, -0x100, -0x48, 0x100, -0x48);

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_fade_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_fade_primitives[i].tpage = GetTPage(1, 2, 0x200, 0);
        g_open_gfx_fade_primitives[i].clut = GetClut(0, 0x1e1);
        setRGB0(prim, 0, 0, 0);
        setUV4(prim, 0, 0, 0xff, 0, 0, 0xb4, 0xff, 0xb4);
        setXY4(prim, -0x100, -0x5a, 0x100, -0x5a, -0x100, 0x5a, 0x100, 0x5a);
    }

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0x100;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Run one frame of the ending script. Returns 0 once the script has ended and 1
 * while it runs.
 *
 * Updates the text scroll, the XA audio, the OPNTEX sequence and the picture
 * fade, then executes script entries. An entry is a row of 16-bit values at
 * data_base + byte_offset whose first value, the opcode, indexes
 * g_open_opcode_handlers. A handler advances byte_offset itself; one that has to
 * wait clears OPEN_SCRIPT_DISPATCH_CONTINUE to end the frame and is entered again
 * on the next, and opcode 0 clears OPEN_SCRIPT_DISPATCH_RUNNING. */
s32 open_script_init_and_dispatch_entries(void) {
    open_script_dispatch_state_t* state;
    open_script_cursor_t* cursor;
    s32 result;
    s16* entry;
    u32 flags;

    open_script_update_timing_and_record_values();
    open_script_update_xa_audio();
    open_gfx_update_opntex_sequence();
    open_script_update_screen_fade();

    state = &g_open_script_state.dispatch;
    result = state->flags | OPEN_SCRIPT_DISPATCH_CONTINUE;
    state->flags = result;
    if (result & OPEN_SCRIPT_DISPATCH_CONTINUE) {
        cursor = &state->cursor;
        do {
            entry = (s16*)open_script_add_pointer_offset(cursor->data_base, cursor->byte_offset);
            g_open_opcode_handlers[*entry](entry);
            /* Keeping the cursor base live matches retail; the direct state view is four bytes shorter. */
            flags = ((open_script_dispatch_state_t*)((u8*)cursor - offsetof(open_script_dispatch_state_t, cursor)))
                        ->flags;
            if ((flags & OPEN_SCRIPT_DISPATCH_RUNNING) == 0) {
                return 0;
            }
        } while (flags & OPEN_SCRIPT_DISPATCH_CONTINUE);
    }
    return 1;
}

u8* open_script_add_pointer_offset(u8* base, s32 byte_offset) {
    return base + byte_offset;
}

/* Queue the ending script's primitives for one frame while the script runs: the
 * OPNTEX picture quad (unless its fade intensity is 0), the two edge bands and
 * one POLY_FT4 per glyph of every active text record.
 *
 * Glyph positions are relative to the center of the 512x240 screen, and skew
 * shifts the top edge of each glyph. The AddPrim calls pass the contents of
 * otag[15] and otag[8], not their addresses: after ClearOTagR those are the
 * addresses of entries 14 and 7, which is where the primitives are linked. */
void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index) {
    s32 i;
    s32 j;
    s32 prim_index;
    s32 x;
    s32 y;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_RUNNING) {
        if (g_open_script_state.opntex.fade_intensity != 0) {
            AddPrim((void*)otag[15], &g_open_gfx_fade_primitives[graphics_buffer_index]);
        }
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[0]);
        AddPrim((void*)otag[8], &g_open_gfx_credit_band_primitives[1]);
        for (i = 0; i < 16; i++) {
            if (g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE) {
                prim_index = i * 42;
                for (j = 0; j < g_open_script_state.records[i].glyph_count; j++, prim_index++) {
                    x = g_open_script_state.records[i].x + g_open_script_state.records[i].glyphs[j].x - 256;
                    y = g_open_script_state.records[i].y + g_open_script_state.records[i].glyphs[j].y - 120;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x0
                        = x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y0 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x1
                        = g_open_script_state.records[i].glyphs[j].w + x + g_open_script_state.records[i].skew;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y1 = y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x2 = x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y2
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].x3
                        = g_open_script_state.records[i].glyphs[j].w + x;
                    g_open_script_glyph_primitives[graphics_buffer_index][prim_index].y3
                        = g_open_script_state.records[i].glyphs[j].h + y;
                    AddPrim((void*)otag[8], &g_open_script_glyph_primitives[graphics_buffer_index][prim_index]);
                }
            }
        }
    }
}

/* Lays out one OPEN script text line as textured glyph quads.
 *
 * Claims the first free script record, then walks the text until '@' or 42
 * glyphs. Escape characters followed by one hex digit select the font
 * ('$'), CLUT ('^'), pen adjustments ('<', '>', '+'), the record field at
 * 0x08 ('/') and wide-glyph mode ('*'). Every other character emits a glyph
 * record and a POLY_FT4 in both graphics buffers. Returns the record index
 * plus 1, or 0 when all 16 records are busy.
 *
 * `value` holds both the decoded escape digits and the glyph metrics offset:
 * the extra references give it the target's global-allocation priority
 * (s3, ahead of the loop counter). The second metrics pointer is a separate
 * variable so the first stays in a0.
 */
s32 open_script_build_text_record(u8* text) {
    s32 index;
    s32 i;
    s32 drawn;
    s32 pen;
    s32 width;
    s32 height;
    open_font_metrics_t* font;
    open_font_metrics_t* mt;
    open_font_metrics_t* mt2;
    s32 value;
    s32 slot;
    POLY_FT4* back;
    POLY_FT4* front;
    s32 u0;
    s32 v0;
    u8 ch;
    u16 tpage;

    font = g_open_script_font_metrics;
    for (i = 0; i < 16; i++) {
        if (!(g_open_script_state.records[i].flags & OPEN_SCRIPT_RECORD_ACTIVE)) {
            break;
        }
    }
    if (i == 16) {
        return 0;
    }
    index = i;

    g_open_script_state.records[index].flags = OPEN_SCRIPT_RECORD_ACTIVE;
    pen = 0;
    slot = index * 42;
    drawn = 0;
    for (i = 0; i < 42; i++) {
        ch = *text;
        if (ch == '@') {
            break;
        }
        if (ch == '$') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.font_index = value;
            i--;
        } else if (ch == '^') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.clut = GetClut(value * 16, 0x1e0);
            i--;
        } else if (ch == '<') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen -= value;
            i--;
        } else if (ch == '>') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            pen += value;
            i--;
        } else if (ch == '/') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.skew = value;
            i--;
        } else if (ch == '*') {
            text++;
            value = open_text_decode_hex_digit(*text++);
            g_open_script_state.dispatch.wide_glyphs = value;
            i--;
        } else if (ch == '+') {
            text++;
            pen = open_text_decode_hex_digit(*text++) * 20;
            i--;
        } else {
            value = (ch - 0x20) * 8 + g_open_script_state.dispatch.font_index * 0x380;
            /* Typed pointer addition reverses this retail addu's operands. */
            mt = (open_font_metrics_t*)(value + (s32)font);
            if (mt->flags & 1) {
                text++;
                i--;
                pen += mt->width;
            } else {
                g_open_script_state.records[index].glyphs[drawn].x = pen - mt->bearing;
                g_open_script_state.records[index].glyphs[drawn].y = -mt->descender;
                width = mt->bearing + mt->width;
                height = mt->height + mt->descender - 1;
                g_open_script_state.records[index].glyphs[drawn].w = width;
                g_open_script_state.records[index].glyphs[drawn].h = height;
                if (g_open_script_state.dispatch.wide_glyphs == 0) {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x * 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w * 2;
                } else {
                    g_open_script_state.records[index].glyphs[drawn].x
                        = g_open_script_state.records[index].glyphs[drawn].x
                        + g_open_script_state.records[index].glyphs[drawn].x / 2;
                    g_open_script_state.records[index].glyphs[drawn].w
                        = g_open_script_state.records[index].glyphs[drawn].w
                        + g_open_script_state.records[index].glyphs[drawn].w / 2;
                }
                front = &g_open_script_glyph_primitives[0][slot];
                g_open_script_state.records[index].skew = g_open_script_state.dispatch.skew;
                SetPolyFT4(front);
                back = &g_open_script_glyph_primitives[1][slot];
                SetPolyFT4(back);
                SetSemiTrans(front, 1);
                SetSemiTrans(back, 1);
                tpage = g_open_script_state.dispatch.tpage;
                front[16 * 42].tpage = tpage;
                g_open_script_glyph_primitives[0][slot].tpage = tpage;
                drawn++;
                tpage = g_open_script_state.dispatch.clut;
                front[16 * 42].clut = tpage;
                g_open_script_glyph_primitives[0][slot].clut = tpage;
                setRGB0(front, 0x80, 0x80, 0x80);
                setRGB0(back, 0x80, 0x80, 0x80);
                mt2 = font + (value >> 3);
                u0 = mt2->u;
                v0 = mt2->v - mt2->height - mt2->descender + 1;
                setUV4(front, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                setUV4(back, u0, v0, u0 + width, v0, u0, v0 + height, u0 + width, v0 + height);
                text++;
                slot++;
                pen += mt2->advance;
            }
        }
    }
    g_open_script_state.records[index].glyph_count = drawn;
    return index + 1;
}

/* Scroll the ending script's text.
 *
 * timing_step is the speed in 1/256 pixel per frame; a negative step scrolls up.
 * The whole pixels moved are added to the y of every active text record and, as a
 * distance, to the external counter opcode 6 waits on. A record that leaves y
 * 0x10..0x100 is released. */
void open_script_update_timing_and_record_values(void) {
    s32 step;
    s32 work;
    s32 index;

    step = g_open_script_state.dispatch.timing_step;
    if (step == 0) {
        return;
    }

    if (step > 0) {
        work = step + g_open_script_timing_fraction;
        g_open_script_timing_fraction = work & 0xff;
        work >>= 8;
        g_open_script_external_counter += work;
    } else {
        work = g_open_script_timing_fraction - step;
        g_open_script_timing_fraction = work & 0xff;
        work >>= 8;
        g_open_script_external_counter += work;
        work = -work;
    }

    for (index = 0; index < 16; index++) {
        if (g_open_script_state.records[index].flags & OPEN_SCRIPT_RECORD_ACTIVE) {
            g_open_script_state.records[index].y += work;
            if (g_open_script_state.records[index].y < 0x10 || g_open_script_state.records[index].y > 0x100) {
                g_open_script_state.records[index].flags ^= OPEN_SCRIPT_RECORD_ACTIVE;
            }
        }
    }
}

/* The last sector of OPEN/ENDING.XA (DISC_SECTOR_OPEN_ENDING_XA is its first). */
#define ENDING_XA_LAST_SECTOR 0x3586c

/* Runs the XA audio state machine once per frame.
 *
 * Starting: sets the drive mode to 0x68 (real-time ADPCM, sub-header filter,
 * 2340-byte sectors at single speed), seeks to ENDING.XA, sets the filter to
 * file 1 and the channel in control_parameter (command 0xd, CdlSetfilter),
 * starts a streaming read and waits for it to be acknowledged; control_state
 * counts the steps and open_file_get_cd_sync_state_delta advances it past
 * each command and back one step on a disc error. A seek (step 5) still
 * unfinished after 120 frames resets the CD subsystem and starts over.
 * Playing: from two seconds in, restarts the sequence if the drive stopped
 * reading, and stops once the head (command 0x10, CdlGetlocL) has passed the
 * file's last sector.
 * Stopping: pauses the drive, restores double speed and brings the CD
 * volume back for the streams. */
void open_script_update_xa_audio(void) {
    u8 result[8];
    CdlFILTER filter;
    u8 mode;

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
        switch (g_open_script_state.control_state) {
        case 0:
            mode = CdlModeRT | CdlModeSF | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 4:
            CdIntToPos(DISC_SECTOR_OPEN_ENDING_XA, &g_main_file_cd_state.position);
            CdControlF(CdlSeekL, (u8*)&g_main_file_cd_state.position);
            {
                s32* state = &g_open_script_state.control_state;

                g_main_system_session_frames = 0;
                (*state)++;
            }
            break;
        case 5: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
            if (g_main_system_session_frames >= 0x79 && *state == 5) {
                main_file_reset_cd_subsystems();
                *state = 0;
            }
        } break;
        case 6:
            filter.file = 1;
            {
                s32 parameter = g_open_script_state.control_parameter;

                filter.chan = parameter;
            }
            CdControlF(CdlSetfilter, (u8*)&filter);
            g_open_script_state.control_state++;
            break;
        case 1:
        case 7: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 8:
            CdControlF(CdlReadS, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 9:
            if (open_file_get_cd_sync_state_delta() == 1) {
                u32* control = &g_open_script_state.dispatch.flags;

                g_open_script_state.xa_wait_frames = 0;
                *control = (*control | OPEN_SCRIPT_DISPATCH_XA_PLAYING) ^ OPEN_SCRIPT_DISPATCH_XA_STARTING;
            }
            break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }

    {
        u32* control = &g_open_script_state.dispatch.flags;

        if (*control & OPEN_SCRIPT_DISPATCH_XA_PLAYING) {
            g_open_script_state.xa_wait_frames++;
            if (g_open_script_state.xa_wait_frames < 0x78) {
                return;
            }
            if (!(CdStatus() & CdlStatRead)) {
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STARTING;
                return;
            }
            if (CdControl(CdlGetlocL, 0, result) != 0 && CdPosToInt((CdlLOC*)result) > ENDING_XA_LAST_SECTOR) {
                open_sound_set_cd_mix_and_volume(0, 1);
                g_open_script_state.control_state = 0;
                *control = (*control ^ OPEN_SCRIPT_DISPATCH_XA_PLAYING) | OPEN_SCRIPT_DISPATCH_XA_STOPPING;
                return;
            }
        }
    }

    if (g_open_script_state.dispatch.flags & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
        switch (g_open_script_state.control_state) {
        case 0:
            CdControlF(CdlPause, 0);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 2:
            mode = CdlModeSpeed;
            CdControlF(CdlSetmode, &mode);
            {
                s32* state = &g_open_script_state.control_state;

                (*state)++;
            }
            break;
        case 1:
        case 3: {
            s32 delta = open_file_get_cd_sync_state_delta();
            s32* state = &g_open_script_state.control_state;
            s32 value = *state;

            *state = value + delta;
        } break;
        case 6: {
            u32* control = &g_open_script_state.dispatch.flags;

            *control ^= OPEN_SCRIPT_DISPATCH_XA_STOPPING;
            open_sound_set_cd_mix_and_volume(0xc0, 1);
        } break;
        default: {
            s32* state = &g_open_script_state.control_state;

            (*state)++;
        } break;
        }
    }
}

/* Value of a hexadecimal digit character, or 0 for any other character. */
s32 open_text_decode_hex_digit(s32 character) {
    u32 value;

    value = character - '0';
    if (value < 10) {
        return value;
    }

    if (character >= 'A' && character < 'G') {
        return character - ('A' - 10);
    }

    if (character >= 'a' && character < 'g') {
        return character - ('a' - 10);
    }

    return 0;
}

/*
 * Advance the script-controlled screen fade and tint the active fade quad.
 * Block-local control pointers retain the original materialized flag addresses.
 */
void open_script_update_screen_fade(void) {
    s32 intensity;

    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            intensity = (elapsed_frames << 7) / duration_frames;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            if (intensity >= 0x81) {
                intensity = 0x80;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0x80;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
            }
        }
    }
    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            intensity = 0x80 - (elapsed_frames << 7) / duration_frames;
            if (intensity < 0) {
                intensity = 0;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            }
        }
    }

    intensity = g_open_script_state.opntex.fade_intensity;
    setRGB0(&g_open_gfx_fade_primitives[g_active_graphics_buffer_index], intensity, intensity, intensity);
}

/* Advance the OPNTEX sequence: run down the fade delay of the frame on
 * screen, and when the frame countdown expires load the next frame and start
 * its fade (or stop the sequence after the last frame). */
void open_gfx_update_opntex_sequence(void) {
    s32 frame;
    u32* source;

    if (g_open_script_state.opntex.frame_duration == 0) {
        return;
    }

    if (g_open_script_state.opntex.delay != 0) {
        g_open_script_state.opntex.delay--;
        if (g_open_script_state.opntex.delay == 0) {
            g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = 0;
            g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
        }
    }

    if (--g_open_script_state.opntex.frame_countdown != 0) {
        return;
    }

    frame = g_open_script_state.opntex.first_frame;
    if (g_open_script_state.opntex.last_frame < frame) {
        g_open_script_state.opntex.frame_duration = 0;
        return;
    }

    source = g_open_gfx_opntex_data;
    source += frame * 23 * 512;
    open_gfx_load_opntex_into_frame_buffer(source);
    g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
    g_open_script_state.opntex.first_frame++;
    g_open_script_state.opntex.frame_countdown = g_open_script_state.opntex.frame_duration;
    g_open_script_state.opntex.delay
        = g_open_script_state.opntex.frame_duration - g_open_script_state.opntex_control_value;
}

/* Start FFTEND.STR (sector 0x21908, ending at frame 0x571) as the first
 * controller. */
void open_movie_play_end(void) {
    g_open_current_controller_index = 0;
    g_active_graphics_buffer_index = 0;
    open_movie_stream_fftend(DISC_SECTOR_OPEN_FFTEND_STR, 0x3858, 0x571);
}

/* Start the ending movie at CD mix level 0x7e and push handler 9. The second
 * argument, the stream's length in sectors, is not read. */
void open_movie_stream_fftend(s32 start_sector, s32 unused, s32 end_frame) {
    s32 controller;

    (void)unused;
    open_movie_start_stream(start_sector, end_frame, -1, 0x7e);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_FFTEND_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* Handler 9, the FFTEND.STR ending movie: Start stops the stream. Once the stream
 * has stopped, by itself or on the update after Start, the display is blanked and
 * the controller is replaced by the ending script. */
void open_movie_finish_fftend_controller_if_ready_or_skipped(open_controller_record_t* unused_record) {
    u32 flags;
    s32 controller;

    if (!(g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING)) {
        SetDispMask(0);
        flags = g_open_system_runtime_flags;
        controller = g_open_current_controller_index;
        flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        controller--;
        g_open_system_runtime_flags = flags;
        g_open_current_controller_index = controller;
        open_script_push_controller();
        return;
    }

    if (g_open_input_new_button_presses & PSX_PAD_START) {
        open_movie_stop_stream(1);
    }
}

/* Start the ending script: leave the 320-pixel and 24-bit modes, make each
 * frame's background the picture copied from VRAM
 * (OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM), reset the script state, which sets
 * up the 512-pixel screen, select VSync mode 2 and push handler 10. */
void open_script_push_controller(void) {
    s32 controller;

    g_open_system_runtime_flags &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24);
    open_gfx_init_screen_environments(1);
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM;
    open_script_init_state();

    controller = g_open_current_controller_index;
    g_open_system_vsync_mode = 2;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_SCRIPT;
    g_open_current_controller_index = controller + 1;
}

/* Handler 10: run the ending script for a frame. When it ends, VSync mode 0 is
 * restored, the controller is popped and OPEN.BIN's loop ends. */
void open_script_update_controller(open_controller_record_t* unused_record) {
    if (open_script_init_and_dispatch_entries() == 0) {
        g_open_system_vsync_mode = 0;
        g_open_current_controller_index--;
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
    }
}

/* Opcode 0: end the script. It yields for the frame while the XA start or stop
 * sequence runs; after that it clears OPEN_SCRIPT_DISPATCH_RUNNING, which stops
 * the dispatcher. Two bytes. */
void open_opcode_toggle_control(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & (OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
    } else {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_RUNNING;
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    }
}

/* Opcode 1: create and position a text record from an ending-script command
 * (opcode, x, y, text offset). Eight bytes.
 *
 * A coordinate of -1 centers the record on its last glyph's extent. X bit 0x100
 * instead centers the glyph selected by its low byte. */
void open_opcode_create_positioned_text_record(s16* command) {
    s32 x;
    s32 y;
    s32 index;

    command++;
    x = *command++;
    y = *command++;
    index = open_script_build_text_record(
        open_script_add_pointer_offset(g_open_script_state.dispatch.cursor.data_base, *command));
    if (index != 0) {
        index--;
        if (x == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            x = 256
                - (g_open_script_state.records[index].glyphs[last].x
                      + g_open_script_state.records[index].glyphs[last].w)
                    / 2;
        }
        if (y == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            y = 120
                - (g_open_script_state.records[index].glyphs[last].y
                      + g_open_script_state.records[index].glyphs[last].h)
                    / 2;
        }
        if (x & 0x100) {
            s32 glyph = x & 0xFF;

            x = 0x100 - g_open_script_state.records[index].glyphs[glyph].x
                - g_open_script_state.records[index].glyphs[glyph].w / 2;
        }
        g_open_script_state.records[index].x = x;
        g_open_script_state.records[index].y = y;
    }
    g_open_script_state.dispatch.cursor.byte_offset += 8;
}

/* Opcode 2: set the text scroll speed to command[1] (1/256 pixel per frame,
 * negative for upward) and zero the fraction and the scrolled-distance counter.
 * Four bytes. */
void open_opcode_set_timing_step(s16* entry) {
    const s16* command = entry;
    s32 value = command[1];
    s32 next = g_open_script_byte_offset + 4;

    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = value;
    g_open_script_byte_offset = next;
}

/* Opcode 3: reveal the screen over command[1] frames, with the overlay in
 * ordering-table entry 2. Four bytes. */
void open_opcode_start_overlay_fade_out(s16* command) {
    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_out(*command);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 4: fade the screen to black over command[1] frames, with the overlay in
 * ordering-table entry 2. Four bytes. */
void open_opcode_start_overlay_fade_in(s16* command) {
    command++;
    g_open_gfx_overlay_fade.ot_index = 2;
    open_gfx_start_overlay_fade_in(*command);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 5: wait command[1] frames. The first call arms the wait and each later
 * call counts one frame, yielding until the count is reached. Four bytes. */
void open_opcode_wait_counted(s16* entry) {
    u32 flags;
    s32 counter;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        counter = state->wait_counter + 1;
        state->wait_counter = counter;
        if (counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->wait_counter = 0;
    }
}

/* Opcode 6: wait until the text has scrolled command[1] pixels, counted from the
 * call that arms the wait. Four bytes. */
void open_opcode_wait_for_external_counter(s16* entry) {
    u32 flags;
    open_script_dispatch_state_t* state;

    state = &g_open_script_state.dispatch;
    flags = state->flags;
    entry++;
    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state->external_counter >= *entry) {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
            state->cursor.byte_offset = state->cursor.byte_offset + 4;
        } else {
            state->flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        }
    } else {
        state->flags = (flags | OPEN_SCRIPT_DISPATCH_WAITING) ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        state->external_counter = 0;
    }
}

/* Opcode 7: start channel command[1] of ENDING.XA and wait until the start
 * sequence of open_script_update_xa_audio has finished. The CD volume is dropped
 * to 0 first. The opcode is skipped when XA audio is already playing, starting or
 * stopping. Four bytes. */
void open_opcode_start_xa_audio(s16* command) {
    u32 flags = g_open_script_state.dispatch.flags;

    if (flags & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (flags & OPEN_SCRIPT_DISPATCH_XA_STARTING) {
            g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
            return;
        }
        g_open_script_state.dispatch.flags = flags ^ OPEN_SCRIPT_DISPATCH_WAITING;
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    if (flags
        & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.cursor.byte_offset += 4;
        return;
    }
    g_open_script_state.control_parameter = command[1];
    g_open_script_state.control_state = 0;
    g_open_script_state.dispatch.flags = (flags
                                             & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING
                                                 | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING))
        | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STARTING);
    open_sound_set_cd_mix_and_volume(0, 1);
}

/* Opcode 8: fade the CD audio in at mix level 0x7a over command[1] * 4 ticks.
 * Four bytes.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_in(s16* command) {
    open_sound_set_cd_mix_and_volume(0x7a, command[1] * 4);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 9: fade the CD audio out over command[1] * 4 ticks. Four bytes.
 *
 * `command` is not const, matching the script handler type: GCC 2.7.2 marks a
 * load through a const pointer unchanging and would schedule it above the
 * prologue's ra save. */
void open_opcode_start_sound_fade_out(s16* command) {
    open_sound_set_cd_mix_and_volume(0, command[1] * 4);
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 10: stop the XA audio and wait until the stop sequence of
 * open_script_update_xa_audio has finished. The opcode is skipped when XA audio
 * is neither playing nor starting. Two bytes. */
void open_opcode_stop_xa_audio(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state & OPEN_SCRIPT_DISPATCH_WAITING) {
        if (state & OPEN_SCRIPT_DISPATCH_XA_STOPPING) {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
        } else {
            g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_WAITING;
            g_open_script_state.dispatch.cursor.byte_offset += 2;
        }
    } else if (!(state & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STARTING))) {
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    } else {
        g_open_script_state.control_state = 0;
        g_open_script_state.dispatch.flags
            = (state
                  & ~(OPEN_SCRIPT_DISPATCH_CONTINUE | OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING
                      | OPEN_SCRIPT_DISPATCH_XA_STARTING))
            | (OPEN_SCRIPT_DISPATCH_WAITING | OPEN_SCRIPT_DISPATCH_XA_STOPPING);
        open_sound_set_cd_mix_and_volume(0, 1);
    }
}

/* Each OPNTEX frame occupies 23 * 0x800 bytes of the loaded data. */
#define OPEN_OPNTEX_FRAME_SECTORS 23

/* Opcode 11: upload frame command[1] of the loaded OPNTEX data as the picture and
 * set its fade intensity to 0, which hides it. Four bytes. */
void open_opcode_load_opntex_frame(s16* command) {
    open_gfx_load_opntex_into_frame_buffer(
        g_open_gfx_opntex_data + command[1] * OPEN_OPNTEX_FRAME_SECTORS * (0x800 / sizeof(u32)));
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 12: fade the OPNTEX picture in over command[1] frames (intensity 0 to
 * 0x80). Four bytes. */
void open_opcode_start_fade_intensity_increase(s16* command) {
    s32 value = command[1];

    g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.opntex.fade_duration_frames = value;
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 13: fade the OPNTEX picture out over command[1] frames (intensity 0x80
 * to 0). Four bytes. */
void open_opcode_start_fade_intensity_decrease(s16* command) {
    s32 value = command[1];

    g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0x80;
    g_open_script_state.opntex.fade_duration_frames = value;
    g_open_script_state.dispatch.cursor.byte_offset += 4;
}

/* Opcode 14: yield each frame until the XA audio is neither playing, starting nor
 * stopping. Two bytes. */
void open_opcode_wait_for_xa_audio_idle(s16* unused_entry) {
    u32 state = g_open_script_state.dispatch.flags;

    if (state
        & (OPEN_SCRIPT_DISPATCH_XA_PLAYING | OPEN_SCRIPT_DISPATCH_XA_STOPPING | OPEN_SCRIPT_DISPATCH_XA_STARTING)) {
        g_open_script_state.dispatch.flags = state ^ OPEN_SCRIPT_DISPATCH_CONTINUE;
    } else {
        g_open_script_state.dispatch.cursor.byte_offset += 2;
    }
}

/* Opcode 15: start a slideshow of OPNTEX frames command[1] to command[2], each on
 * screen for command[3] frames with fades in and out of command[4] frames. The
 * first frame is loaded on the next update. Ten bytes. */
void open_opcode_start_opntex_sequence(s16* command) {
    command++;
    g_open_script_state.opntex.first_frame = *command++;
    g_open_script_state.opntex.last_frame = *command++;
    g_open_script_state.opntex.frame_duration = *command++;
    g_open_script_state.opntex_control_value = *command++;
    g_open_script_state.opntex.frame_countdown = 1;
    g_open_script_state.dispatch.cursor.byte_offset += 10;
}

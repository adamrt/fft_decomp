/* Title screen controllers, including file loading, the OPNTEX frame buffer, movie streams, the title menu, sound type,
 * the new game transition and the birthday date and confirmation menus. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/open.h"
#include "fft/open_gfx.h"
#include "fft/open_movie.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_card.h"
#include "fft/world_menu.h"
#include "fft/world_name_entry.h"
#include "fft/world_sprite.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void open_birthday_build_menu_text(open_birthday_date_state_t* menu);
static void open_birthday_push_confirmation_controller(void);
static void open_birthday_push_date_controller(void);
static void open_menu_push_sound_type_controller(void);

/* Load the title screen's graphics: upload the three TIMs at sectors 0, 0x11 and
 * 0x22 of OPNTEX.BIN, then read OPNBK image 0 as the frame image. */
void open_file_load_main_menu_graphics(void) {
    main_file_load_descriptor_t* header = &g_main_file_cd_state;

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN, 0x11, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x11, 0x11, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x22, 0x21, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_build_openbk_header(0, g_open_file_destination);
    file_wait_for_pending();
}

/* Load the ending's graphics from OPNTEX.BIN and OPNBK.BIN: the TIM at sector
 * 0x65 (the script's glyph page, uploaded at once), OPNBK image 6 (uploaded as
 * the 512x240 picture at VRAM (512, 256)) and the 16 OPNTEX frames from sector
 * 0x76, which stay at g_open_gfx_opntex_data for the script to upload. */
void open_file_load_end_movie_graphics(void) {
    main_file_load_descriptor_t* header = &g_main_file_cd_state;
    RECT frame;

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x65, 0x11, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_build_openbk_header(6, g_open_gfx_opntex_data);
    file_wait_for_pending();

    frame.x = 0x200;
    frame.y = 0x100;
    frame.w = 0x200;
    frame.h = 0xf0;
    LoadImage(&frame, g_open_gfx_opntex_data);
    DrawSync(0);

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x76, 0x170, g_open_gfx_opntex_data);
    file_wait_for_pending();
}

/* Upload the two TIMs at sectors 0x43 and 0x54 of OPNTEX.BIN: the text pages at
 * VRAM (256, 256) and (320, 256) that the new-game prologue draws its lines from.
 * The only caller is open_title_start_new_game_transition. */
static void open_file_load_prologue_text_pages(void) {
    main_file_load_descriptor_t* header = &g_main_file_cd_state;

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x43, 0x11, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN + 0x54, 0x11, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);
}

/* Start reading image image_id of OPNBK.BIN into destination, unless it is the
 * image read last (g_open_file_current_openbk_image_id). The read is
 * asynchronous; only a read already pending is waited for.
 *
 * The end-offset table is the start-offset table one entry further on. By their
 * sector counts images 0 and 1 are 320x240 and 256x240 frame images, 2 to 5 are
 * 210x180 pictures and 6 is the 512x240 ending picture. */
void open_file_build_openbk_header(s32 image_id, void* destination) {
    s32 start_offset;
    s32 stack_padding[2];

    if (image_id == g_open_file_current_openbk_image_id) {
        return;
    }

    g_open_file_current_openbk_image_id = image_id;
    start_offset = g_open_file_openbk_start_sector_offsets[image_id];
    file_wait_then_build_header(&g_main_file_cd_state, DISC_SECTOR_OPEN_OPNBK_BIN + start_offset,
        g_open_file_openbk_end_sector_offsets[image_id] - start_offset, destination);
}

/* Poll the pending CD command: 1 when it has completed, -1 on a disc error and 0
 * while it runs. open_script_update_xa_audio adds the result to its step, so
 * completion advances the sequence and an error repeats the previous step. */
s32 open_file_get_cd_sync_state_delta(void) {
    s32 status = CdSync(1, 0);

    if (status == 2) {
        return 1;
    }

    return -(status == 5);
}

/* Upload an OPNTEX image, preceded by its CLUT when flag bit 3 is set.
 *
 * The second 32-bit value holds the flags. Each block is a byte length (including itself), the
 * VRAM position packed as y << 16 | x, the size packed as h << 16 | w, then
 * the pixel data. The CLUT rectangle is flattened to one row of w * h
 * entries. */
void open_gfx_load_opntex_into_frame_buffer(u32* tim) {
    RECT rect;
    s32 offset;
    /* Unused; the target's frame is 16 bytes larger than rect needs. */
    s32 stack_padding[4];

    /* Packed 32-bit RECT reads preserve the retail lw/lhu mix. */
    if (tim[1] & 8) {
        offset = (tim[2] >> 2) + 2;
        rect.x = tim[3];
        rect.y = tim[3] >> 16;
        rect.w = tim[4];
        rect.h = tim[4] >> 16;
        rect.w = rect.w * rect.h;
        rect.h = 1;
        LoadImage(&rect, tim + 5);
    } else {
        offset = 2;
    }
    rect.x = tim[offset + 1];
    rect.y = tim[offset + 1] >> 16;
    rect.w = tim[offset + 2];
    rect.h = tim[offset + 2] >> 16;
    LoadImage(&rect, &tim[offset + 3]);
    DrawSync(0);
}

/* First controller of the title sequence: the FFTST.STR logo movie (sector
 * 0x153d6, 0x278a sectors; the stream ends at frame 0x3df and the video alone at
 * frame 0x2de, so its CD audio plays on under the title menu), or the title
 * menu at once when use_alternate is set. */
void open_movie_start_fftst_or_title_menu(s32 use_alternate) {
    g_open_current_controller_index = 0;
    if (use_alternate == 0) {
        g_main_gfx_active_buffer_index = 0;
        open_movie_stream_fftst_and_push_controller(DISC_SECTOR_OPEN_FFTST_STR, 0x278A, 0x3DF, 0x2DE);
    } else {
        open_title_push_menu_controller(0);
    }
}

/* Run the current controller c: handler
 * g_open_controller_handlers[g_open_controller_handler_indices[c - 1]] on record
 * g_open_controller_records[c]. */
void open_controller_dispatch_current(void) {
    s32 controller = g_open_current_controller_index;

    g_open_controller_handlers[g_open_controller_handler_indices[controller - 1]](
        &g_open_controller_records[controller]);
}

/* Start the logo movie at CD mix level 0x76 and push handler 0, whose record
 * keeps the stream's first sector and length for the title menu's CD end
 * position. */
void open_movie_stream_fftst_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 video_end_frame) {
    s32 controller;

    open_movie_start_stream(start_sector, end_frame, video_end_frame, 0x76);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].movie.start_sector = start_sector;
    g_open_controller_records_next[controller].movie.stream_length = stream_length;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_FFTST_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* Handler 0, the FFTST.STR logo movie: when the stream has ended, or on Start
 * (which stops the video, blanks the display and leaves the CD audio playing),
 * the controller is replaced by the title menu. The menu is given the sector just
 * past the stream, from the record's start sector and length, as the place to
 * stop the CD audio. */
void open_movie_finish_stream_controller_if_ready_or_skipped(open_controller_record_t* record) {
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) {
        if (!(g_open_input_new_button_presses & PSX_PAD_START)) {
            return;
        }

        open_movie_stop_stream(0);
        SetDispMask(0);
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    }

    g_open_current_controller_index--;
    open_title_push_menu_controller(record->movie.start_sector + record->movie.stream_length);
}

/* Start the opening movie (the one caller passes the sector of OPEN/FFTOP.STR) with its CD audio
 * at mix level 0x94, and push its controller. */
static void open_movie_stream_fftop_and_push_controller(s32 start_sector, s32 unused_stream_length, s32 end_frame) {
    s32 controller;

    (void)unused_stream_length;
    open_movie_start_stream(start_sector, end_frame, -1, 0x94);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_FFTOP_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* OPEN_CONTROLLER_FFTOP_MOVIE: Start stops the opening movie; once it has stopped the controller
 * pops, blanks the display and ends OPENING with result 0. */
void open_movie_finish_fftop_controller_if_ready_or_skipped(open_controller_record_t* unused_record) {
    u32 flags;
    s32 controller;

    if (!(g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING)) {
        SetDispMask(0);
        flags = g_open_system_runtime_flags;
        controller = g_open_current_controller_index;
        g_open_system_result = 0;
        flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        controller--;
        g_open_system_runtime_flags = flags;
        flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
        g_open_current_controller_index = controller;
        g_open_system_runtime_flags = flags;
        return;
    }

    if (g_open_input_new_button_presses & PSX_PAD_START) {
        open_movie_stop_stream(1);
    }
}

/* Title-menu view of g_open_controller_records_next; indexing it as an array of
 * this type (rather than casting the controller-record array) is required for
 * the target's constant-folded field addresses. */
extern open_title_controller_t g_open_title_menu_states[];

/* Build the title menu (logo, four items and cursor) and push its controller.
 *
 * The target builds the records base and the x/y view in v0 and copies them
 * into their saved registers. That needs the two short-lived temporaries
 * entry and field: each is referenced again later in the function, so CSE
 * keeps them as the canonical copies, and local allocation then rewrites
 * their remaining setup uses to records and positions. */
void open_title_push_menu_controller(s32 argument) {
    RECT rect;
    s32 unused[4];
    s32 i;
    s32 record;
    s32 y;
    s32 controller;
    wldcore_window_xy_t* positions;
    point32_t* position;
    world_animated_render_record_t* records;
    world_animated_render_record_t* entry;
    s32* field;

    g_open_system_runtime_flags
        = (g_open_system_runtime_flags & ~OPEN_SYSTEM_RUNTIME_FLAG_RGB24) | OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
    open_gfx_init_screen_environments(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xf0;
    if (g_main_gfx_active_buffer_index != 0) {
        rect.y = 0xf0;
    }
    LoadImage(&rect, g_open_file_destination);
    DrawSync(0);

    record = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    entry = g_open_gfx_render_records_36;
    records = entry;
    field = &records->position.x;
    positions = (wldcore_window_xy_t*)field;
    g_open_title_menu_states[g_open_current_controller_index].records_36[0] = record;
    g_open_gfx_render_records_36[record].sequence = 0x11;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    positions[record].x = 0;
    positions[record].y = 0x64;

    for (i = 0, y = 0x1e; i < 4; i++) {
        record
            = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
        g_open_title_menu_states[g_open_current_controller_index].records_36[i + 1] = record;
        g_open_gfx_render_records_36[record].sequence = i + 3;
        g_open_gfx_render_records_36[record].priority = 4;
        records[record].anim_counter = 0;
        g_open_gfx_render_records_36[record].frame_index = 0;
        positions[record].y = y;
        y += 0xc;
        positions[record].x = -0x18;
    }

    record = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_title_menu_states[g_open_current_controller_index].cursor_record_36 = record;
    entry = &g_open_gfx_render_records_36[record];
    entry->sequence = 0x13;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    g_open_gfx_render_records_36[record].flags |= WORLD_RENDER_RECORD_HIDDEN;
    position = &g_open_gfx_render_records_36[record].position;
    position->x = 0x60;
    position->y = 0x58;

    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED) {
        open_gfx_start_overlay_fade_out(0x20);
    }

    controller = g_open_current_controller_index;
    g_open_title_menu_states[controller].cursor = OPEN_TITLE_MENU_NEW_GAME;
    g_open_title_menu_states[controller].idle_timer = 0;
    g_open_title_menu_states[controller].cd_end_position = argument;
    g_open_title_menu_states[controller].state = OPEN_TITLE_STATE_MENU;
    g_open_title_menu_states[controller].exiting = 0;
    record = g_open_title_menu_states[controller].records_36[g_open_title_menu_states[controller].cursor + 1];
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
    field = &g_open_controller_handler_indices[controller];
    *field = OPEN_CONTROLLER_TITLE_MENU;
}

/* Show the four title menu entries. */
static void open_gfx_show_four_render_records(open_title_controller_t* title) {
    g_open_gfx_render_records_36[title->records_36[1]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[2]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[3]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[4]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
}

/* Hide the four title menu entries. */
static void open_gfx_hide_four_render_records(open_title_controller_t* title) {
    g_open_gfx_render_records_36[title->records_36[1]].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[2]].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[3]].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[4]].flags |= WORLD_RENDER_RECORD_HIDDEN;
}

/* Per-frame handler for the OPEN title menu controller.
 *
 * While the CD audio plays it watches the current sector and stops the drive
 * at the record's end position. Once the audio has stopped and the menu has
 * been idle for 601 frames it fades out and starts the next demo movie.
 * Circle or Start confirms the cursor entry: New Game, Continue and Tutorial
 * fade out and then dispatch (Continue runs the memory card screen and ends
 * OPENING with 0 or 1 for a loaded save, Tutorial ends it with 2), while
 * Sound opens the sound type menu without fading. Up and Down (and Select)
 * move the highlight between the four entries.
 */
void open_title_update_menu(open_controller_record_t* record) {
    open_title_controller_t* state = &record->title;
    u8 loc[8];
    s32 status;
    s32 remaining;
    s32 flags;
    s32 demo;
    s32 start_sector;
    s32 stream_length;
    s32 end_frame;
    s32 cd_mix_level;

    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
        return;
    }

    /* Stop the CD audio as the exit timer runs out: pause at 3, then hold at 2 until the
     * pause completes, pausing again after a disk error (status 5). */
    if (state->exiting != 0) {
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) {
            state->exit_timer--;
            if (state->exit_timer == 3) {
                CdControl(CdlPause, 0, 0);
            }

            /* remaining is 2 here, which is also CdSync's complete status. */
            remaining = state->exit_timer;
            if (remaining == 2) {
                status = CdSync(1, loc);
                if (status == 5) {
                    CdControl(CdlPause, 0, 0);
                } else if (status != remaining) {
                    state->exit_timer++;
                }
            }
            if (state->exit_timer != 0) {
                return;
            }
            state->exiting = 0;
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING;
            return;
        }
        state->exiting = 0;
        return;
    }

    /* At the track's end position, fade the CD mix out and start the exit countdown. */
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) {
        CdControlB(CdlGetlocL, 0, loc);
        if (CdPosToInt((CdlLOC*)loc) >= state->cd_end_position) {
            open_sound_set_cd_mix_and_volume(0, 0x20);
            state->exit_timer = 0x20;
            state->exiting = 1;
            return;
        }
    }

    if (state->state == OPEN_TITLE_STATE_DEMO_MOVIE) {
        if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
            return;
        }
        /* Drop the menu's six render records and this controller, then play the next of three demos. */
        g_main_gfx_active_buffer_index = 1;
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        g_open_gfx_next_render_record_36 -= 6;
        g_open_gfx_render_record_pointer_count -= 6;
        demo = g_open_title_demo_movie_index;
        g_open_current_controller_index--;
        switch (demo) {
        case 1:
            start_sector = DISC_SECTOR_OPEN_FFTPR_STR;
            stream_length = 0x1d60;
            end_frame = 0x2b7;
            cd_mix_level = 0x7a;
            break;
        case 2:
            start_sector = DISC_SECTOR_OPEN_FFTUNIT_STR;
            stream_length = 0x8691;
            end_frame = 0xd4d;
            cd_mix_level = 0x73;
            break;
        default:
            start_sector = DISC_SECTOR_OPEN_FFTPRE_STR;
            stream_length = 0x38d0;
            end_frame = 0x54e;
            cd_mix_level = 0x85;
            break;
        }

        open_movie_stream_title_demo_and_push_controller(start_sector, stream_length, end_frame, cd_mix_level);
        g_open_title_demo_movie_index++;
        if (g_open_title_demo_movie_index >= 3) {
            g_open_title_demo_movie_index = 0;
        }
        return;
    }

    if (state->state == OPEN_TITLE_STATE_SOUND_MENU) {
        state->state = OPEN_TITLE_STATE_MENU;
        open_gfx_hide_four_render_records(state);
        open_menu_push_sound_type_controller();
        return;
    }

    /* Input is disabled while a confirm fades out; once the fade ends, act on the chosen entry. */
    flags = g_open_system_runtime_flags;
    if (flags & OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED) {
        s32 cursor;

        if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
            return;
        }

        g_open_system_runtime_flags = flags ^ OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        open_movie_pause_cd_audio();
        g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
        open_gfx_init_screen_environments(1);
        cursor = state->cursor;
        switch (cursor) {
        case OPEN_TITLE_MENU_NEW_GAME:
            g_main_debug_display_enabled = 0;
            g_open_system_runtime_flags
                &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED | OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN);
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            open_title_select_new_game();
            return;

        case OPEN_TITLE_MENU_CONTINUE:
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            open_title_init_new_game_party(1, 1);
            DrawSync(0);
            VSync(0);
            SetDispMask(1);

            /* Nothing was loaded: rebuild the title screen and its menu controller. */
            if (world_card_run_menu_screen(0) == 0) {
                g_open_file_current_openbk_image_id = -1;
                open_file_build_openbk_header(0, g_open_file_destination);
                file_wait_for_pending();
                g_open_gfx_next_render_record_36 -= 6;
                g_open_gfx_render_record_pointer_count -= 6;
                g_open_current_controller_index--;
                SetDispMask(0);
                g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
                open_title_push_menu_controller(0);
                return;
            }

            /* A loaded save ends OPENING with 1 (the cursor) or, when it has a next scenario
             * and no formation return event, with 0. */
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
            if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
                || script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
                g_open_system_result = cursor;
                return;
            }
            g_open_system_result = 0;
            return;

        case OPEN_TITLE_MENU_TUTORIAL:
            open_title_init_new_game_party(0, 1);
            g_open_system_result = 2;
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_TITLE_TUTORIAL;
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
            return;
        }
        return;
    }

    /* After 601 idle frames with the CD audio stopped, fade out into a demo movie. */
    state->idle_timer++;
    if (!(flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) && state->idle_timer >= 0x259) {
        state->state = OPEN_TITLE_STATE_DEMO_MOVIE;
        g_open_gfx_overlay_fade.ot_index = 2;
        open_gfx_start_overlay_fade_in(0x20);
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        open_sound_set_cd_mix_and_volume(0, 0x64);
        state->exit_timer = 0x32;
        state->exiting = 1;
        main_sound_play_sfx(MAIN_SFX_CONFIRM);

        /* These three fade out first; the dispatch above runs once the fade ends. */
        switch (state->cursor) {
        case OPEN_TITLE_MENU_NEW_GAME:
        case OPEN_TITLE_MENU_CONTINUE:
        case OPEN_TITLE_MENU_TUTORIAL:
            g_open_gfx_overlay_fade.ot_index = 2;
            open_gfx_start_overlay_fade_in(0x20);
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
            return;
        case OPEN_TITLE_MENU_SOUND:
            state->state = OPEN_TITLE_STATE_SOUND_MENU;
            return;
        }
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette = 0;
        state->cursor--;
        if (state->cursor < 0) {
            state->cursor = OPEN_TITLE_MENU_SOUND;
        }
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette
            = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if (open_input_update_repeat(PSX_PAD_DOWN) != 0 || (g_open_input_new_button_presses & PSX_PAD_SELECT)) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette = 0;
        state->cursor++;
        if (state->cursor >= 4) {
            state->cursor = OPEN_TITLE_MENU_NEW_GAME;
        }
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette
            = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Build the three-entry sound-type menu and select the saved option.
 *
 * The target reaches three fields through a pointer to the record or to its
 * x member (base register plus offset) and the others as plain array members
 * (absolute address plus index); the spellings below keep that split. The
 * selected render-record index is likewise read through the controller
 * record, while sound_mode_selection and step are addressed through
 * g_open_controller_selected_options. */
static void open_menu_push_sound_type_controller(void) {
    s32 i;
    s32 record;
    /* Pin required: unpinned, the controller index and the stream base swap
     * $a0 and $a1. */
    register s32 controller_index __asm__("$4");
    s32 sound_type;
    s32 selected_record;
    point32_t* position;
    open_sound_menu_state_t* menu;

    for (i = 0; i < OPEN_SOUND_MENU_ENTRY_COUNT; i++) {
        record
            = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
        g_open_controller_records_next[g_open_current_controller_index].sound_menu.entry_render_indices[i] = record;
        g_open_gfx_render_records_36[record].priority = 4;
        g_open_gfx_render_records_36[record].sequence = i + 7;
        (&g_open_gfx_render_records_36[record])->anim_counter = 0;
        g_open_gfx_render_records_36[record].frame_index = 0;
        position = &g_open_gfx_render_records_36[record].position;
        position->y = i * 0xc + 0x1e;
        position->x = -0x14;
    }

    g_open_controller_selected_options[g_open_current_controller_index * 25 + 1] = 0;
    sound_type = suzuki_sound_get_type();
    controller_index = g_open_current_controller_index;
    g_open_controller_selected_options[controller_index * 25] = sound_type;
    menu = &g_open_controller_records_next[controller_index].sound_menu;
    sound_type <<= 2;
    /* The explicit byte offset preserves the target's index shift before the
     * menu-base addition; direct array indexing schedules those instructions
     * in the opposite order. */
    sound_type += (s32)menu;
    selected_record = *(s32*)sound_type;
    g_open_current_controller_index = controller_index + 1;
    g_open_gfx_render_records_36[selected_record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
    g_open_controller_handler_indices[controller_index] = OPEN_CONTROLLER_SOUND_TYPE_MENU;
}

/* Per-frame handler of the sound type menu (handler 3).
 *
 * Circle or Start applies the highlighted mode (mono, stereo or wide) with
 * suzuki_sound_set_type and stores it in the game options; Cross leaves the mode as
 * it is. Both set step 2, and the next update releases the three option records,
 * pops the controller, shows the title menu's four entries again and zeroes the
 * title controller's idle timer. Up, and Down or Select, move the highlight and
 * wrap around. */
void open_menu_handle_sound_type_input(open_controller_record_t* record) {
    open_sound_menu_state_t* menu = &record->sound_menu;
    s32 render_record;

    if (menu->step == 2) {
        g_open_gfx_next_render_record_36 -= OPEN_SOUND_MENU_ENTRY_COUNT;
        g_open_gfx_render_record_pointer_count -= OPEN_SOUND_MENU_ENTRY_COUNT;
        g_open_current_controller_index -= 1;
        open_gfx_show_four_render_records(&g_open_controller_records[g_open_current_controller_index].title);
        /* The title menu underneath, addressed from the pushed-record base. */
        g_open_controller_records_next[g_open_current_controller_index - 1].title.idle_timer = 0;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        suzuki_sound_set_type(menu->sound_mode_selection);
        g_main_game_options.fields.sound_mode = menu->sound_mode_selection;
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        menu->step = 2;
        return;
    }
    if (g_open_input_new_button_presses & PSX_PAD_CROSS) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        menu->step = 2;
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = 0;
        menu->sound_mode_selection -= 1;
        if (menu->sound_mode_selection < SUZUKI_SOUND_TYPE_MONO) {
            menu->sound_mode_selection = SUZUKI_SOUND_TYPE_WIDE;
        }
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if (open_input_update_repeat(PSX_PAD_DOWN) != 0 || (g_open_input_new_button_presses & PSX_PAD_SELECT) != 0) {
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = 0;
        menu->sound_mode_selection += 1;
        if (menu->sound_mode_selection > SUZUKI_SOUND_TYPE_WIDE) {
            menu->sound_mode_selection = SUZUKI_SOUND_TYPE_MONO;
        }
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Begin the new-game prologue: start MAIN_MUSIC_NEW_GAME with the master volume
 * ramping up from 0, upload the prologue's text pages, read OPNBK image 1 as the
 * frame image, request the 12-entry sequence g_open_new_game_transition_entries
 * with its text in ordering-table entries 3 and 4, and push the new-game
 * controller (handler 4) at step 0 as the screen is revealed. */
void open_title_start_new_game_transition(void) {
    s32 controller;
    s32 stack_padding[4];

    suzuki_sound_set_master_volume(0, 1);
    main_sound_set_current_music_track(MAIN_MUSIC_NEW_GAME);
    suzuki_sound_set_master_volume(0x3fff, 5);
    open_file_load_prologue_text_pages();
    open_file_build_openbk_header(1, g_open_file_destination);
    file_wait_for_pending();
    open_gfx_init_transition_request(12, 3, g_open_new_game_transition_entries);
    open_gfx_start_overlay_fade_out(0x20);

    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].new_game.step = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_NEW_GAME_START;
    g_open_current_controller_index = controller + 1;
}

/* Per-frame handler of the new-game controller (handler 4).
 *
 * Until step 4 it waits for the overlay fade and runs the prologue
 * (open_gfx_update_transition_sequence) to its end. Step 4 builds the new-game
 * party (loading WORLD if needed), copies the displayed frame into the frame
 * image and the other buffer, runs the WORLD name-entry screen, stores the
 * 16-byte name in party slot 0 and replaces itself with the birthday date
 * controller.
 *
 * Step 5 unloads the music and starts the music test controller. Nothing in
 * OPEN.BIN stores 5 in step. */
void open_title_step_new_game_start(open_controller_record_t* record) {
    open_title_new_game_state_t* state = &record->new_game;
    RECT rect;
    u8* name;
    party_unit_t* party;

    if (state->step == 5) {
        if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
            return;
        }
        main_sound_unload_current_scenario_music();
        g_open_current_controller_index -= 1;
        open_menu_start_music_test_controller();
        return;
    }

    if (state->step == 4) {
        open_title_init_new_game_party(0, 0);
        world_menu_init_with_open_text();

        /* Copy the displayed buffer into the frame image, then into the draw buffer. */
        if (g_main_gfx_active_buffer_index != 0) {
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
        } else {
            rect.x = 0;
            rect.y = 0xF0;
            rect.w = 0x100;
            rect.h = 0xF0;
        }
        StoreImage(&rect, g_open_file_destination);
        DrawSync(0);

        if (g_main_gfx_active_buffer_index == 0) {
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
        } else {
            rect.x = 0;
            rect.y = 0xF0;
            rect.w = 0x100;
            rect.h = 0xF0;
        }
        LoadImage(&rect, g_open_file_destination);
        DrawSync(0);

        g_open_file_current_openbk_image_id = -1;
        name = world_name_run_entry_screen(text_find_entry(TEXT_ID_UNIT_NAME_SPECIAL_BASE), g_open_file_destination);
        party = main_party_get_unit(0);
        memcpy(party->name, name, sizeof(party->name));
        g_open_current_controller_index -= 1;
        open_birthday_push_date_controller();
        return;
    }

    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
        return;
    }
    if (open_gfx_update_transition_sequence() != 0) {
        return;
    }
    open_gfx_transition_finished_stub();
    state->step = 4;
}

/* Pushes the birthday date-entry controller (handler 5,
 * open_birthday_handle_menu_input) with its cursor record and the date prompt
 * record, then seeds month/day to 1 and builds the menu text.
 *
 * Shaped like open_birthday_push_confirmation_controller: the controller record
 * is reached through g_open_controller_records_next[g_open_current_controller_index]
 * on every use (the target reloads the index after each call rather than
 * keeping it in a local), and the prompt record's position is written through
 * the records_56 x/y view so the base register is set before the index scale.
 *
 * One `record` variable serves both the 36- and the 56-byte allocation; two
 * variables let the first live in v0 and drop s2 from the frame. `s32
 * unused[4]` is the 0x10-byte frame hole the declared locals cannot explain,
 * giving the target's 0x48-byte frame. `half_width` is its own statement
 * before the position stores, which hoists the `lhu` of quad.w into the
 * load-delay slot instead of emitting a nop.
 */
static void open_birthday_push_date_controller(void) {
    RECT quad;
    s32 unused[4];
    s32 record;
    s32 controller;
    s32 half_width;
    open_render_record_56_position_t* position;

    record = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.cursor_record_36 = record;
    g_open_gfx_render_records_36[record].sequence = 0xC;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.title_record_56 = record;
    quad.x = 0;
    quad.y = 0;
    quad.w = 0x80;
    quad.h = 0x40;
    window_init_text_record(record, quad, 0, 0xB800, g_open_birthday_title_image);
    position = g_open_gfx_record_translation;
    position += record;
    half_width = quad.w / 2;
    position->y = -0x60;
    position->x = -half_width;
    g_open_gfx_render_records_56[record].ot_layer = 4;
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.month = 1;
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.day = 1;
    open_birthday_build_menu_text(&g_open_controller_records_next[g_open_current_controller_index].birthday_date);

    controller = g_open_current_controller_index;
    g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_date.cursor_record_36].position.x
        = g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].x
        + 0x46;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_date.cursor_record_36].position.y
        = g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].y
        + 0x18;
    g_open_controller_records_next[controller].birthday_date.selecting_month = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_BIRTHDAY_DATE;
}

/* Rebuild the selected birthday's month and day in the lower menu window.
 *
 * The month label is centered from its measured width; both entries are drawn
 * into one scratch image before it is uploaded to VRAM. */
static void open_birthday_build_menu_text(open_birthday_date_state_t* menu) {
    RECT quad;
    point32_t position;
    s16 width;
    u16 height;
    s32 index = window_append_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    s32 text_id;
    open_render_record_56_position_t* translation;
    s32* substitution;

    menu->window_record_56 = index;
    quad.x = 0;
    quad.y = 0x40;
    quad.w = 0x50;
    quad.h = 0x20;
    open_gfx_init_window_frame_record(index, quad, 0, (u16*)g_open_birthday_window_image);

    /* The month names are text entries 0xB803 (January) to 0xB80E. Entry 0xB801
     * prints the substituted month name and 0xB802 the substituted day. */
    text_id = menu->month + 0xB802;
    text_measure_pixels(&width, &height, text_find_entry(text_id));

    position.x = 0x1C - width / 2;
    position.y = 8;
    substitution = g_text_substitution_values;
    *substitution = text_id;
    window_display_text_entry(index, 0xB801, position, g_open_birthday_window_image);

    position.x = 0x30;
    position.y = 8;
    *substitution = menu->day;
    window_display_text_entry(index, 0xB802, position, g_open_birthday_window_image);

    open_gfx_load_window_record_image(index, (u32*)g_open_birthday_window_image);

    translation = g_open_gfx_record_translation;
    translation += index;
    translation->x = -(quad.w / 2);
    translation->y = -0x10;
    g_open_gfx_render_records_56[index].ot_layer = 4;
    g_open_gfx_render_records_56[index].flags &= ~WORLD_RENDER_RECORD_POP_IN;
}

/* Restore the date-selection records after leaving the confirmation prompt. */
static void open_birthday_restore_date_menu(open_birthday_date_state_t* menu) {
    open_render_record_56_t* records_56 = g_open_gfx_render_records_56;
    /* The target reserves a 24-byte frame it never touches, as in the twin
     * open_birthday_build_confirmation_menu. */
    s32 unused_scratch[6];
    s32 cursor;
    s32 cursor_last;
    open_render_record_56_t* window;
    open_render_record_56_t* title;

    g_open_gfx_render_records_36[menu->cursor_record_36].sequence = 12;

    cursor = menu->cursor_record_36;
    g_open_gfx_render_records_36[cursor].anim_counter = 0;
    g_open_gfx_render_records_36[cursor].frame_index = 0;

    g_open_gfx_render_records_36[menu->label_record].color.r = 0x80;
    g_open_gfx_render_records_36[menu->label_record].color.g = 0x80;
    g_open_gfx_render_records_36[menu->label_record].color.b = 0x80;

    window = &records_56[menu->window_record_56];
    cursor_last = menu->cursor_record_36;
    title = &records_56[menu->title_record_56];
    window->palette = 0;
    title->palette = 0;
    g_open_gfx_render_records_36[cursor_last].palette = 0;
}

/* Recolor the birthday menu's render records for the confirmation prompt:
 * the date cursor takes sequence 10 and palette 7, the label is grayed, and
 * the title and date windows take palette 2 (the CLUT 16 entries right of
 * tail.clut).
 *
 * The target reserves a 24-byte frame it never touches (the stack adjustment
 * is even scheduled into the last load-delay slot), so the original body
 * declared an unused aggregate local of 17..24 bytes. */
static void open_birthday_build_confirmation_menu(open_birthday_date_state_t* state) {
    s32 unused_scratch[6];

    g_open_gfx_render_records_36[state->cursor_record_36].sequence = 10;
    g_open_gfx_render_records_36[state->cursor_record_36].frame_index
        = g_open_gfx_render_records_36[state->cursor_record_36].anim_counter = 0;

    g_open_gfx_render_records_36[state->label_record].color.r = 0x40;
    g_open_gfx_render_records_36[state->label_record].color.g = 0x40;
    g_open_gfx_render_records_36[state->label_record].color.b = 0x40;

    g_open_gfx_render_records_36[state->cursor_record_36].palette = 7;

    g_open_gfx_render_records_56[state->title_record_56].palette = 2;
    g_open_gfx_render_records_56[state->window_record_56].palette = 2;
}

#define OPEN_BIRTHDAY_MONTH_COUNT 12
#define OPEN_BIRTHDAY_CURSOR_STEP 0x20

/* Per-frame handler of the birthday date-entry controller (handler 5).
 *
 * Input is ignored while the date window is popping in. Start or Circle recolors
 * the menu for the prompt and pushes the confirmation controller. Up and Down
 * (with auto-repeat) step the selected field: a day rolls over into the next or
 * previous month, a month wraps between December and January and clamps the day
 * to the new month's length. Each change releases the date window record (the
 * last one allocated) and rebuilds it. Left selects the month field and Right the
 * day field, moving the cursor 0x20 pixels.
 *
 * February has 28 days in g_unit_birthday_month_lengths, so 29 February cannot be
 * entered. */
void open_birthday_handle_menu_input(open_controller_record_t* record) {
    open_birthday_date_state_t* menu = &record->birthday_date;
    if (g_open_gfx_render_records_56[menu->window_record_56].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_START | PSX_PAD_CIRCLE)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_birthday_build_confirmation_menu(menu);
        open_birthday_push_confirmation_controller();
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        if (menu->selecting_month == 0) {
            menu->day += 1;
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
                menu->month += 1;
                if (menu->month > OPEN_BIRTHDAY_MONTH_COUNT) {
                    menu->month = 1;
                }
                menu->day = 1;
            }
        } else {
            menu->month += 1;
            if (menu->month > OPEN_BIRTHDAY_MONTH_COUNT) {
                menu->month = 1;
            }
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    } else if (open_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (menu->selecting_month == 0) {
            menu->day -= 1;
            if (menu->day <= 0) {
                menu->month -= 1;
                if (menu->month <= 0) {
                    menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
                }
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        } else {
            menu->month -= 1;
            if (menu->month <= 0) {
                menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
            }
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_LEFT) && menu->selecting_month == 0) {
        menu->selecting_month = 1;
        g_open_gfx_render_records_36[menu->cursor_record_36].position.x -= OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_RIGHT) && menu->selecting_month != 0) {
        menu->selecting_month = 0;
        g_open_gfx_render_records_36[menu->cursor_record_36].position.x += OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Pushes the birthday confirmation controller (handler 6,
 * open_birthday_handle_confirmation_input) with its two cursor records and
 * the confirmation prompt window.
 *
 * The pushed record is written member by member through the record array:
 * only direct array indexing keeps the target's per-field absolute
 * addressing. The prompt window's position is written through the
 * records_56 x/y view, which sets the base register before the index scale.
 * The unused array reproduces the target's 0x70-byte frame.
 */
static void open_birthday_push_confirmation_controller(void) {
    RECT quad;
    s32 unused[12];
    s32 record;
    s32 controller;
    open_render_record_56_position_t* position;

    record = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.cursor_record_36 = record;
    g_open_gfx_render_records_36[record].sequence = 2;
    g_open_gfx_render_records_36[record].priority = 3;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_animated_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.label_record = record;
    g_open_gfx_render_records_36[record].sequence = 0xE;
    g_open_gfx_render_records_36[record].priority = 3;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_record(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.menu_record_56 = record;
    quad.x = 0;
    quad.y = 0x60;
    quad.w = 0x28;
    quad.h = 0x30;
    window_init_text_record(record, quad, 0, 0xB80F, g_open_birthday_title_image);
    controller = g_open_current_controller_index;
    position = g_open_gfx_record_translation;
    position += record;
    position->x = 0x3C;
    position->y = 0x28;
    g_open_gfx_render_records_56[record].ot_layer = 3;

    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.cursor_record_36]
        .position.x = g_open_gfx_render_records_56[record].x + 4;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.cursor_record_36]
        .position.y = g_open_gfx_render_records_56[record].y + 0xC;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.label_record]
        .position.x = g_open_gfx_render_records_56[record].x + 2;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.label_record]
        .position.y = g_open_gfx_render_records_56[record].y - 2;
    g_open_controller_records_next[controller].birthday_confirmation.selected_option = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_BIRTHDAY_CONFIRMATION;
}

/* Handle the final birthday prompt. Circle on the first option stores Ramza's birthday and zodiac
 * and fades out; once the fade has finished, the birthday controllers are released and the
 * FFTOP.STR opening movie starts. Circle on the second option, like Cross, returns to the date
 * menu. Up and Down toggle the option.
 *
 * The zodiac byte holds bit 8 of the day of the year in bit 0 and the sign (0 = Aries) in the high
 * nibble.
 */
void open_birthday_handle_confirmation_input(open_controller_record_t* record) {
    open_birthday_confirmation_state_t* menu = &record->birthday_confirmation;
    s32 month;
    s32 day;
    s32 value;
    party_unit_t* member;
    u32 buttons;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED)
        && !(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
        /* Release both birthday controllers and their six render records. */
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        main_sound_unload_current_scenario_music();
        g_main_gfx_active_buffer_index = 1;
        g_open_gfx_next_render_record_56 -= 3;
        g_open_gfx_next_render_record_36 -= 3;
        g_open_gfx_render_record_pointer_count -= 6;
        g_open_current_controller_index -= 2;
        open_movie_stream_fftop_and_push_controller(DISC_SECTOR_OPEN_FFTOP_STR, 0x64D8, 0x9E4);
        return;
    }

    buttons = g_open_input_new_button_presses;
    if (buttons & PSX_PAD_CIRCLE) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (menu->selected_option == 0) {
            /* The date controller sits below this one on the controller stack. */
            month = g_open_controller_records_next[g_open_current_controller_index - 2].birthday_date.month;
            day = g_open_controller_records_next[g_open_current_controller_index - 2].birthday_date.day;
            script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH, month);
            script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY, day);
            value = unit_birthday_calculate_day_of_year(month, day);
            open_birthday_convert_date_to_zodiac(&month, &day);
            member = main_party_get_unit(0);
            member->birthday_day = value;
            member->zodiac
                = ((value & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) | ((month - 1) << PARTY_ZODIAC_SHIFT);
            main_sound_set_current_music_target(0, 0x60);
            open_gfx_start_overlay_fade_in(0x20);
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
            return;
        }
        /* Release this controller's three render records and return to the date menu. */
        g_open_gfx_render_record_pointer_count -= 3;
        g_open_gfx_next_render_record_56 -= 1;
        g_open_gfx_next_render_record_36 -= 2;
        g_open_current_controller_index -= 1;
        open_birthday_restore_date_menu(&g_open_controller_records[g_open_current_controller_index].birthday_date);
        return;
    }
    if (buttons & PSX_PAD_CROSS) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        /* Same cancel path as above; GCC cross-jumps the two copies into this one. */
        g_open_gfx_render_record_pointer_count -= 3;
        g_open_gfx_next_render_record_56 -= 1;
        g_open_gfx_next_render_record_36 -= 2;
        g_open_current_controller_index -= 1;
        open_birthday_restore_date_menu(&g_open_controller_records[g_open_current_controller_index].birthday_date);
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0 || open_input_update_repeat(PSX_PAD_DOWN) != 0) {
        menu->selected_option ^= 1;
        /* Adding 12 before the option offset keeps the target's (option * 16 + 12) + y evaluation. */
        g_open_gfx_render_records_36[menu->cursor_record_36].position.y
            = g_open_gfx_render_records_56[menu->menu_record_56].y + 12 + menu->selected_option * 16;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Initialize a 56-byte render record as a window frame and build its image.
 *
 * `tail` and `color` are pointers because the target materializes
 * &records[index].tail and &records[index].color as values, while every other
 * field is stored directly off the record array. `half` and `scaled` keep
 * the target's x / 4 + (64 * depth + 0x180) grouping. */
void open_gfx_init_window_frame_record(s32 index, RECT quad, s32 depth, u16* image) {
    open_render_record_56_tail_t* tail;
    CVECTOR* color;
    s32 half;
    s32 scaled;

    g_open_gfx_render_records_56[index].flags
        &= ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN);
    g_open_gfx_render_records_56[index].x = quad.x - 0x80;
    g_open_gfx_render_records_56[index].y = quad.y - 0x78;
    g_open_gfx_render_records_56[index].depth = depth;
    tail = &g_open_gfx_render_records_56[index].tail;
    tail->clut.y = 0x1E0;
    tail->clut.x = 0;
    g_open_gfx_render_records_56[index].tail.quad = quad;
    half = quad.x / 4;
    scaled = depth * 64 + 0x180;
    g_open_gfx_render_records_56[index].vram_x = half + scaled;
    g_open_gfx_render_records_56[index].vram_y = quad.y;
    color = &g_open_gfx_render_records_56[index].color;
    color->b = color->g = color->r = 0x80;
    g_open_gfx_render_records_56[index].pop_in_step = 0;
    world_menu_build_window_frame_image(quad.w, quad.h, image);
}

/* Upload a window record's image to its place in VRAM.
 *
 * The record's quad is in 4-bit texels within its texture page, four to a
 * VRAM halfword, so the image goes to x = quad.x / 4 + 0x180 + 64 * depth
 * (texture page 6 + depth) with width quad.w / 4. */
void open_gfx_load_window_record_image(s32 record_index, u32* image) {
    RECT destination;
    s32 x;
    s32 depth;
    s32 scaled;

    x = g_open_gfx_render_records_56[record_index].tail.quad.x / 4;
    depth = g_open_gfx_render_records_56[record_index].depth;
    scaled = (depth << 6) + 0x180;
    destination.x = x + scaled;
    destination.y = g_open_gfx_render_records_56[record_index].tail.quad.y;
    destination.w = g_open_gfx_render_records_56[record_index].tail.quad.w / 4;
    destination.h = g_open_gfx_render_records_56[record_index].tail.quad.h;
    LoadImage(&destination, image);
}

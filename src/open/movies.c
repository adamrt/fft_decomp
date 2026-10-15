/* The main loop and ending cutscene, MDEC movie streaming and frame decoding, fade overlays, input repeat, new game
 * party init and birthday zodiac conversion. */

#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define OPEN_OTAG_BUFFER_BYTES 64

/* Entry point of OPEN.BIN for the title sequence. Runs until a controller clears
 * OPEN_SYSTEM_RUNTIME_FLAG_RUNNING and returns g_open_system_result: 0 for a new
 * game, or a loaded save with a next scenario and no formation return event,
 * which main_system_run_game_loop takes straight into BATTLE; 1 for any other
 * loaded save and 2 for the tutorial, which go to WORLD.
 *
 * skip_movie starts at the title menu instead of the FFTST.STR logo movie. Each
 * frame polls the CD read, clears the ordering table and queues the overlay fade,
 * steps the movie, runs the WORLD menu frame while it is on and then the current
 * controller, draws the render records, reads the pad and flips the buffers. */
s32 open_system_run_main_loop(s32 skip_movie) {
    u32* otag_0;
    u32* otag_1;

    open_movie_init_and_stream_opening(skip_movie);
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_open_gfx_otags[0];
        otag_1 = g_open_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            main_file_poll_load(&g_main_file_cd_state);
            open_gfx_clear_and_draw_current_frame();
            open_movie_update_stream_frame();
            open_menu_update_world_formation();
            open_controller_dispatch_current();

            graphics_buffer_index = g_active_graphics_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_0);
            window_dispatch_record_list_variant(
                current_otag, g_open_gfx_render_record_pointers, g_open_gfx_render_record_pointer_count);

            graphics_buffer_index = g_active_graphics_buffer_index;
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            DrawSync(0);
            open_input_update_buttons_and_check_game_reset();
            VSync(0);
            open_movie_present_frame();
            main_system_frame_stub();
        } while ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    SetDispMask(0);
    open_sound_set_cd_mix_and_volume(0xc0, 0x78);
    return g_open_system_result;
}

/* Each graphics buffer's ordering table holds 16 32-bit entries. */
#define OPEN_OTAG_BUFFER_BYTES 64

/* Entry point of OPEN.BIN for the ending: play FFTEND.STR, then run the ending
 * script until it finishes.
 *
 * Each frame polls the CD read, clears the ordering table, steps the movie,
 * queues the script's primitives, runs the current controller, reads the pad and
 * calls VSync with g_open_system_vsync_mode (0 during the movie, 2 while the
 * script runs). The ordering table is drawn after the buffer flip, on top of the
 * background open_movie_present_frame has just put into the new draw buffer. On
 * exit the VRAM page saved by open_movie_init_and_play_end is restored, the
 * display is blanked and the CD audio returns to mix level 0xc0. */
void open_system_run_ending_cutscene(void) {
    u32* otag_0;
    u32* otag_1;

    open_movie_init_and_play_end();
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_open_gfx_otags[0];
        otag_1 = g_open_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            main_file_poll_load(&g_main_file_cd_state);
            open_gfx_clear_and_draw_current_frame();
            open_movie_update_stream_frame();
            graphics_buffer_index = g_active_graphics_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * OPEN_OTAG_BUFFER_BYTES + (u32)otag_0);
            open_script_draw_text_records(current_otag, graphics_buffer_index);
            open_controller_dispatch_current();
            DrawSync(0);
            open_input_update_buttons_and_check_game_reset();
            VSync(g_open_system_vsync_mode);
            open_movie_present_frame();
            current_otag = (u32*)((g_active_graphics_buffer_index ^ 1) * OPEN_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            main_system_frame_stub();
        } while ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    open_gfx_restore_frame_vram_page();
    SetDispMask(0);
    open_sound_set_cd_mix_and_volume(0xc0, 0x78);
}

/* Starts streaming a movie. The stream stops with the CD audio paused at end_frame, and
 * without touching the audio at video_end_frame (-1: none). */
void open_movie_start_stream(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level) {
    open_sound_set_cd_mix_and_volume(0, 1);
    suzuki_sound_set_type(suzuki_sound_get_type());
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24;
    open_gfx_init_screen_environments(1);

    g_open_movie_aligned_height = 0;
    g_open_movie_width = 0;
    g_open_movie_current_frame = 0;
    g_open_movie_stream_status = OPEN_MOVIE_STREAM_PLAYING;
    g_open_movie_end_frame = end_frame;
    g_open_movie_video_end_frame = video_end_frame;
    open_movie_init_mdec_stream_state(&g_open_movie_mdec_stream_state, 0, 8, 0, 0xf8);

    for (;;) {
        open_movie_init_stream(sector, open_movie_handle_mdec_output_slice);
        while (CdRead2(0x1e0) == 0) { }
        if (open_movie_decode_next_frame(&g_open_movie_mdec_stream_state) != 0) {
            break;
        }
        main_file_reset_cd_subsystems();
    }

    open_sound_set_cd_mix_and_volume(cd_mix_level, 0x20);
    PutDrawEnv(&g_open_gfx_screen_environments[0].draw);
    PutDispEnv(&g_open_gfx_screen_environments[0].disp);
    g_open_file_current_openbk_image_id = -1;
    g_open_system_runtime_flags
        = (g_open_system_runtime_flags
              & ~(OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED | OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN))
        | (OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING | OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING);
}

/* Per-frame movie step while a stream plays.
 *
 * When the ring has fewer than 16 free sectors the stream is masked up to the
 * frame StGetBackloc reports and the read restarts at that location. The MDEC is
 * then started on the bitstream decoded last frame and on the first output slice,
 * the next frame's bitstream is decoded, and the routine waits for the picture to
 * be complete. The stream stops when the frame header reached end_frame (pausing
 * the CD audio) or video_end_frame (leaving it playing). */
void open_movie_update_stream_frame(void) {
    CdlLOC position;
    s16 free_sectors;
    s16 read_sectors;
    s32 words;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        return;
    }

    StRingStatus(&free_sectors, &read_sectors);
    if (free_sectors < 16) {
        StSetMask(1, StGetBackloc(&position), -1);
        open_movie_start_cd_stream_read(&position);
    }

    DecDCTin(g_open_movie_mdec_stream_state.vlc_buffers[g_open_movie_mdec_stream_state.vlc_buffer_index], 3);
    words = g_open_movie_mdec_stream_state.output_rect.w;
    words *= g_open_movie_mdec_stream_state.output_rect.h;
    DecDCTout(
        g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index], words / 2);
    open_movie_decode_next_frame(&g_open_movie_mdec_stream_state);
    open_movie_wait_for_mdec_output(&g_open_movie_mdec_stream_state, 0);

    if (g_open_movie_stream_status == OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO) {
        open_movie_stop_stream(1);
    }
    if (g_open_movie_stream_status == OPEN_MOVIE_STREAM_STOP) {
        open_movie_stop_stream(0);
    }
}

void open_movie_stop_stream(s32 pause_cd_audio) {
    DrawSync(0);
    DecDCToutCallback(0);
    StUnSetRing();

    if (pause_cd_audio != 0) {
        open_movie_pause_cd_audio();
    }

    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING;
}

/* While the CD audio plays: drop its volume to 0, pause the drive, wait four
 * vertical blanks and clear OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING. */
void open_movie_pause_cd_audio(void) {
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) != 0) {
        open_sound_set_cd_mix_and_volume(0, 1);

        while (CdControlB(CdlPause, 0, 0) == 0) { }

        VSync(4);
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING;
    }
}

#define OPEN_MDEC_SLICE_WIDTH 24

/* Sets the two VLC and image buffers and the two frame positions. Retail
 * stores the slice width into the global state, not through `state`. */
void open_movie_init_mdec_stream_state(open_movie_mdec_stream_state_t* state, s32 x0, s32 y0, s32 x1, s32 y1) {
    state->vlc_buffers[0] = (u32*)g_open_work_buffer_0;
    state->vlc_buffers[1] = (u32*)g_open_work_buffer_1;
    state->vlc_buffer_index = 0;
    state->output_buffers[0] = g_open_movie_mdec_output_buffer_0;
    state->output_buffers[1] = g_open_movie_mdec_output_buffer_1;
    state->output_buffer_index = 0;
    state->frame_rects[0].x = x0;
    state->frame_rects[0].y = y0;
    state->frame_rects[1].x = x1;
    state->frame_rects[1].y = y1;
    state->frame_rect_index = 0;
    state->output_rect.x = x0;
    state->output_rect.y = y0;
    g_open_movie_mdec_stream_state.output_rect.w = OPEN_MDEC_SLICE_WIDTH;
    state->output_ready = 0;
}

/* Prepare the MDEC and the 0x30-sector CD ring for a movie and aim the drive at
 * it: a one-sector read with no destination makes the file loader seek to
 * `sector` and leaves its position in the loader descriptor, which is then
 * set as the read location. */
void open_movie_init_stream(s32 sector, psyq_interrupt_callback_t output_callback) {
    main_file_load_descriptor_t* header;

    DecDCTReset(0);
    DecDCToutCallback(output_callback);
    StSetRing(g_open_movie_cd_stream_ring_buffer, 0x30);
    StSetStream(1, 0, -1, 0, 0);

    header = &g_main_file_cd_state;
    main_file_build_header_nnl(header, sector, 1, 0);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(header);
        VSync(0);
    }

    while (CdControl(CdlSetloc, (u8*)&g_main_file_cd_state.position, 0) == 0) { }
}

/* Uploads the decoded MDEC slice and starts decoding the next one.
 *
 * The rectangle pointer and the output-buffer array are both addressed through
 * the shared stream state, so GCC derives the buffer array (`addiu s1,s0,-0x20`)
 * from the rectangle address. The slice size is built in one variable: its
 * sign extension, product and halving then share the second argument register.
 */
void open_movie_handle_mdec_output_slice(void) {
    RECT* rect;
    s32 size;

    if (g_psyq_cd_stream_interrupt_pending != 0) {
        StCdInterrupt();
        g_psyq_cd_stream_interrupt_pending = 0;
    }
    rect = &g_open_movie_mdec_stream_state.output_rect;
    LoadImage(rect, g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index]);
    g_open_movie_mdec_stream_state.output_buffer_index = !g_open_movie_mdec_stream_state.output_buffer_index;
    rect->x += g_open_movie_mdec_stream_state.output_rect.w;
    if (rect->x < g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].x
            + g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].w) {
        size = g_open_movie_mdec_stream_state.output_rect.w;
        size *= g_open_movie_mdec_stream_state.output_rect.h;
        size /= 2;
        DecDCTout(
            g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index], size);
    } else {
        g_open_movie_mdec_stream_state.output_ready = 1;
        g_open_movie_mdec_stream_state.frame_rect_index = !g_open_movie_mdec_stream_state.frame_rect_index;
        rect->x = g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].x;
        g_open_movie_mdec_stream_state.output_rect.y
            = g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].y;
    }
}

/* Fetch the next movie frame and decode its bitstream into the other VLC buffer,
 * releasing the frame's ring sectors. Returns 0 when no frame arrived. */
s32 open_movie_decode_next_frame(open_movie_mdec_stream_state_t* state) {
    u32* frame_data;

    frame_data = open_movie_acquire_next_frame(state);
    if (frame_data == 0) {
        return 0;
    }
    state->vlc_buffer_index = !state->vlc_buffer_index;
    DecDCTvlc(frame_data, state->vlc_buffers[state->vlc_buffer_index]);
    StFreeRing(frame_data);
    return 1;
}

#define OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS 0x80

/* Wait for the next movie frame (at most 129 vertical blanks), track the
 * stream position, adopt a changed frame size (clearing the screen), and size
 * the decode rectangles: 24-bit rows take three 16-bit values per two pixels.
 * Returns the frame data, or NULL on timeout. */
u32* open_movie_acquire_next_frame(open_movie_mdec_stream_state_t* state) {
    u32* frame_data;
    StHEADER* header;
    s32 attempts;

    attempts = 0;
    /* Passing header through StGetNext's 32-bit data-pointer slot preserves native header reloads. */
    while (StGetNext(&frame_data, (u32**)&header) != 0) {
        VSync(0);
        if (attempts == OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS) {
            return 0;
        }
        attempts++;
    }

    g_open_movie_current_frame = header->frameCount;
    if (header->frameCount >= g_open_movie_end_frame) {
        g_open_movie_stream_status = OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO;
    }
    if (g_open_movie_video_end_frame != -1 && header->frameCount >= g_open_movie_video_end_frame) {
        g_open_movie_stream_status = OPEN_MOVIE_STREAM_STOP;
    }

    if (g_open_movie_width != header->width || g_open_movie_height != header->height) {
        RECT clear_rect;

        clear_rect.x = 0;
        clear_rect.y = 0;
        clear_rect.w = 0x1e0;
        clear_rect.h = 0x1e0;
        ClearImage(&clear_rect, 0, 0, 0);
        g_open_movie_width = header->width;
        g_open_movie_height = header->height;
        g_open_movie_aligned_height = (header->height + 15) & 0xfff0;
    }

    state->frame_rects[0].w = state->frame_rects[1].w = g_open_movie_width * 3 / 2;
    state->frame_rects[0].h = state->frame_rects[1].h = g_open_movie_aligned_height;
    state->output_rect.h = g_open_movie_aligned_height;
    return frame_data;
}

/* Busy-wait until the MDEC output callback has delivered the whole frame
 * (output_ready), then clear the flag. After 0x800000 passes without it the frame
 * is taken as complete and the output rectangle moves to the other frame buffer.
 * The second argument is not read. */
void open_movie_wait_for_mdec_output(open_movie_mdec_stream_state_t* state, s32 unused) {
    /* The timeout is read and written on the stack each iteration. */
    volatile s32 timeout = 0x800000;

    while (state->output_ready == 0) {
        if (--timeout == 0) {
            state->output_ready = 1;
            state->frame_rect_index = !state->frame_rect_index;
            state->output_rect.x = state->frame_rects[state->frame_rect_index].x;
            state->output_rect.y = state->frame_rects[state->frame_rect_index].y;
        }
    }

    state->output_ready = 0;
}

/* Set the read location and start the streaming read (CdRead2 mode 0x1a0),
 * repeating both until the read is accepted. */
void open_movie_start_cd_stream_read(const CdlLOC* location) {
    for (;;) {
        while (CdControl(CdlSetloc, (u8*)location, 0) == 0) { }
        if (CdRead2(0x1a0) != 0) {
            break;
        }
    }
}

/* Flip the frame buffers and repaint the background of the new draw buffer.
 *
 * Applies the other buffer's draw and display environments. Unless a movie is
 * streaming, the 256- or 320-pixel-wide frame is then cleared
 * (OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME), overwritten with the 512x240 picture at
 * VRAM (512, 256) (OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM, the ending script)
 * or uploaded from the frame image at g_open_file_destination. The display is
 * enabled unless OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED is set.
 *
 * Nothing in OPEN.BIN sets OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME. */
void open_movie_present_frame(void) {
    RECT image_rect;
    RECT move_rect;
    s32 display_index;
    u32 flags;

    display_index = g_active_graphics_buffer_index ^ 1;
    g_active_graphics_buffer_index = display_index;
    PutDrawEnv(&g_open_gfx_screen_environments[display_index].draw);

    display_index = g_active_graphics_buffer_index;
    PutDispEnv(&g_open_gfx_screen_environments[display_index].disp);

    flags = g_open_system_runtime_flags;
    if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        image_rect.h = 240;
        image_rect.x = 0;
        if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) != 0) {
            image_rect.w = 320;
        } else {
            image_rect.w = 256;
        }
        if ((g_active_graphics_buffer_index & 1) != 0) {
            image_rect.y = 240;
        } else {
            image_rect.y = 0;
        }

        flags = g_open_system_runtime_flags;
        if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME) != 0) {
            ClearImage(&image_rect, 0, 0, 0);
        } else if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM) != 0) {
            move_rect.x = 512;
            move_rect.y = 256;
            move_rect.w = 512;
            move_rect.h = 240;
            MoveImage(&move_rect, image_rect.x, image_rect.y);
        } else {
            LoadImage(&image_rect, g_open_file_destination);
        }
    }

    SetDispMask(((g_open_system_runtime_flags >> 6) ^ 1) & 1);
}

/* Read the pad and derive the held and newly pressed buttons, both 0 while
 * OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED is set. L1, R1, Select and Start held
 * with nothing else reset the game. OPEN_SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED
 * would prevent that; no code in OPEN.BIN sets it. */
void open_input_update_buttons_and_check_game_reset(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED) {
        g_open_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_open_input_new_button_presses = input & ~previous;
    }

    if (!(g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED)
        && g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        open_sound_set_cd_mix_and_volume(0, 1);
        main_system_reset_game();
    }
}

/* Reveal the screen over `duration` frames: the subtractive overlay fades out. */
void open_gfx_start_overlay_fade_out(u32 duration) {
    u32 flags = g_open_gfx_overlay_fade.flags;

    g_open_gfx_overlay_fade.duration = duration;
    g_open_gfx_overlay_fade.frame = 0;
    g_open_gfx_overlay_fade.flags = flags | (OPEN_OVERLAY_FADE_OUT | OPEN_OVERLAY_FADE_ACTIVE);
}

/* Fade the screen to black over `duration` frames: the subtractive overlay fades
 * in. */
void open_gfx_start_overlay_fade_in(u32 duration) {
    u32 flags = g_open_gfx_overlay_fade.flags;

    g_open_gfx_overlay_fade.duration = duration;
    g_open_gfx_overlay_fade.frame = 0;
    g_open_gfx_overlay_fade.flags = flags | (OPEN_OVERLAY_FADE_IN | OPEN_OVERLAY_FADE_ACTIVE);
}

/* Advance the screen overlay fade and queue its full-screen tile.
 *
 * A fade-out lowers the subtractive level from 0xff to 0 and clears
 * OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED on its second frame; a fade-in raises
 * the level to 0xff and sets the flag on its last frame. While the fade is active
 * the TILE and its DR_MODE go into ordering-table entry ot_index.
 *
 * With a single struct, cse keeps the flags address in a register (the target's
 * `la a3`/`la a2`/`la s1`) and the stores through tiles[] serialize the x/y/w/h
 * 16-bit copies exactly as the target does.  Per-block locals give block 2 its
 * own a2/a1/a0/v1 registers. */
void open_gfx_update_fade_overlay(u32* ot) {
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_OUT) {
        s32 duration = g_open_gfx_overlay_fade.duration;
        s32 frame = g_open_gfx_overlay_fade.frame;
        g_open_gfx_overlay_fade.level = 0xff - (0x100 / duration) * frame;
        if (g_open_gfx_overlay_fade.level < 0) {
            g_open_gfx_overlay_fade.level = 0;
        }
        if (frame == 1) {
            g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_open_gfx_overlay_fade.frame = frame + 1;
        if (duration < frame + 1) {
            g_open_gfx_overlay_fade.flags &= ~(OPEN_OVERLAY_FADE_OUT | OPEN_OVERLAY_FADE_ACTIVE);
        }
    }
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_IN) {
        s32 duration = g_open_gfx_overlay_fade.duration;
        s32 frame = g_open_gfx_overlay_fade.frame;
        g_open_gfx_overlay_fade.level = (0x100 / duration) * frame;
        if (g_open_gfx_overlay_fade.level >= 0x100) {
            g_open_gfx_overlay_fade.level = 0xff;
        }
        if (frame == duration - 1) {
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_open_gfx_overlay_fade.frame = frame + 1;
        if (duration < frame + 1) {
            g_open_gfx_overlay_fade.flags &= ~(OPEN_OVERLAY_FADE_IN | OPEN_OVERLAY_FADE_ACTIVE);
        }
    }
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
        TILE* tiles = g_open_gfx_overlay_fade.tiles;
        SetTile(&tiles[g_active_graphics_buffer_index]);
        SetSemiTrans(&tiles[g_active_graphics_buffer_index], 1);
        SetDrawMode(&g_open_gfx_overlay_fade.modes[g_active_graphics_buffer_index], 1, 0,
            GetTPage(0, g_open_gfx_overlay_fade.abr, 0x100, 0), 0);
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].x0 = g_open_gfx_overlay_fade.rect.x;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].y0 = g_open_gfx_overlay_fade.rect.y;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].w = g_open_gfx_overlay_fade.rect.w;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].h = g_open_gfx_overlay_fade.rect.h;
        setRGB0(&tiles[g_active_graphics_buffer_index], g_open_gfx_overlay_fade.level, g_open_gfx_overlay_fade.level,
            g_open_gfx_overlay_fade.level);
        AddPrim(&ot[g_open_gfx_overlay_fade.ot_index], &tiles[g_active_graphics_buffer_index]);
        AddPrim(&ot[g_open_gfx_overlay_fade.ot_index], &g_open_gfx_overlay_fade.modes[g_active_graphics_buffer_index]);
    }
}

/* Clear the direction repeat counters and take the repeat delay and period from
 * MAIN's input timings. */
void open_input_init_directional_state(void) {
    s32 first_limit = g_main_input_repeat_initial_delay;
    s32 second_limit = g_main_input_repeat_period;

    g_open_input_right_repeat_counter = 0;
    g_open_input_left_repeat_counter = 0;
    g_open_input_down_repeat_counter = 0;
    g_open_input_up_repeat_counter = 0;
    g_open_input_directional_repeat_initial_delay = first_limit;
    g_open_input_directional_repeat_period = second_limit;
}

/* Return the newly pressed buttons in `buttons`, or else Up or Down when that
 * held direction's auto-repeat fires, or 0.
 *
 * A new press clears the four direction counters. A held direction counts once
 * per call; past g_open_input_directional_repeat_initial_delay it fires and steps
 * back by g_open_input_directional_repeat_period. Unlike the WLDCORE twin only Up
 * and Down repeat: the other two counters are cleared and never counted. */
u32 open_input_update_repeat(u32 buttons) {
    s32 next;
    u32 pressed;

    pressed = g_open_input_new_button_presses;
    pressed &= buttons;
    if (pressed != 0) {
        g_open_input_right_repeat_counter = 0;
        g_open_input_left_repeat_counter = 0;
        g_open_input_down_repeat_counter = 0;
        g_open_input_up_repeat_counter = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* up_counter = &g_open_input_up_repeat_counter;

            if (g_open_input_directional_repeat_initial_delay < (*up_counter = next = *up_counter + 1)) {
                *up_counter = next - g_open_input_directional_repeat_period;
                return PSX_PAD_UP;
            }
        } else {
            g_open_input_up_repeat_counter = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* down_counter = &g_open_input_down_repeat_counter;
            s32 old_counter;
            s32 down_next;

            old_counter = *down_counter;
            if (g_open_input_directional_repeat_initial_delay < (*down_counter = down_next = old_counter + 1)) {
                old_counter = down_next - g_open_input_directional_repeat_period;
                *down_counter = old_counter;
                return PSX_PAD_DOWN;
            }
        } else {
            g_open_input_down_repeat_counter = 0;
        }
    }

    return 0;
}

/* Load WORLD if needed, then initialize the selected new-game party. */
void open_title_init_new_game_party(s32 party_mode, s32 world_load_mode) {
    u32 flags;
    u32 party_mode_flag;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED) == 0) {
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED;
        main_overlay_open_world_bin(world_load_mode);
    }

    party_mode_flag = OPEN_PARTY_MODE_FLAG_BASE << party_mode;
    flags = g_open_system_runtime_flags;
    if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN) == 0 || (flags & party_mode_flag) == 0) {
        g_open_system_runtime_flags
            = ((flags | OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN) & ~OPEN_PARTY_MODE_FLAG_MASK) | party_mode_flag;
        main_party_init_new_game(party_mode);
        world_text_generate_formation_unit_name_string();
    }
}

/* Convert a calendar month/day into a zodiac sign number (1 = Aries) and the
 * 1-based day within that sign.
 *
 * `span` first holds the previous sign's start day and then the number of
 * days from it to the end of its month; computing it in two steps keeps GCC
 * from folding the `+ 1` into the sum with `*day`. */
void open_birthday_convert_date_to_zodiac(s32* month, s32* day) {
    s32 index;
    s32 length;
    s32 span;

    index = 0;
    while (g_open_birthday_zodiac_start_dates[index * 2] != *month) {
        index = (index + 1) % ZODIAC_SIGN_ORDINARY_COUNT;
    }
    if (*day < g_open_birthday_zodiac_start_dates[index * 2 + 1]) {
        index = (index + ZODIAC_SIGN_ORDINARY_COUNT - 1) % ZODIAC_SIGN_ORDINARY_COUNT;
        *month = index + 1;
        length = (g_unit_birthday_month_lengths - 1)[g_open_birthday_zodiac_start_dates[index * 2]];
        span = g_open_birthday_zodiac_start_dates[index * 2 + 1];
        span = length - span + 1;
        *day += span;
        return;
    }
    *month = index + 1;
    *day = *day - g_open_birthday_zodiac_start_dates[index * 2 + 1] + 1;
}

/* While OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set (the music test), run
 * one WORLD menu frame with the blocked buttons withheld, drawing from entry
 * g_open_menu_formation_otag_index of the ordering table OPEN is building. The
 * active buffer index is flipped for the duration of the call. */
void open_menu_update_world_formation(void) {
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME) {
        u32 blocked_buttons = g_menu_blocked_buttons;

        g_active_graphics_buffer_index ^= 1;
        world_formation_set_menu_allowed_buttons(~blocked_buttons);
        world_formation_run_menu_frame(
            &g_open_gfx_otags[g_active_graphics_buffer_index ^ 1][g_open_menu_formation_otag_index],
            g_input_current_buttons);
        g_active_graphics_buffer_index ^= 1;
    }
}

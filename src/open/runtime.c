/* OPEN runs the title and ending loops here, with the CD streaming and MDEC
 * pipeline that supplies their movies. The loops also present the background
 * image and poll the pad, including the retail soft-reset chord. */

#include "fft/controller.h"
#include "fft/ending.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/movie.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/title.h"
#include "fft/window.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libpress.h"
#include "psx/pad.h"
#include "psx/types.h"

/* g_movie_stream_status, set as each frame header arrives. */
typedef enum movie_stream_status {
    MOVIE_STREAM_PLAYING = 0,
    MOVIE_STREAM_STOP_AND_PAUSE_AUDIO = 1, /* reached g_movie_end_frame */
    MOVIE_STREAM_STOP = 2                  /* reached g_movie_video_end_frame; the CD audio plays on */
} movie_stream_status_e;

/* Shared VLC/MDEC state used by OPEN.BIN's movie-stream pipeline. */
typedef struct movie_mdec_state {
    u32* vlc_buffers[2];     /* 0x00; decoded run-level data in 32-bit entries */
    s32 vlc_buffer_index;    /* 0x08 */
    u32* output_buffers[2];  /* 0x0c; decoded image data in 32-bit entries */
    s32 output_buffer_index; /* 0x14 */
    RECT frame_rects[2];     /* 0x18 */
    s32 frame_rect_index;    /* 0x28 */
    RECT output_rect;        /* 0x2c */
    s32 output_ready;        /* 0x34 */
} movie_mdec_state_t;
typedef char movie_mdec_state_size_must_be_56[(sizeof(movie_mdec_state_t) == 56) ? 1 : -1];

extern movie_stream_status_e g_movie_stream_status;
extern movie_mdec_state_t g_movie_mdec_stream_state;
extern u32 g_movie_cd_stream_ring_buffer[];
extern u32 g_movie_mdec_output_buffer_0[];
extern u32 g_movie_mdec_output_buffer_1[];
extern s32 g_movie_width;
extern s32 g_movie_height;
extern s32 g_movie_aligned_height;
extern u32 g_movie_current_frame;
extern u32 g_movie_end_frame;
extern s32 g_movie_video_end_frame; /* -1: none */

static void input_update_with_soft_reset(void);
static u32* movie_acquire_next_frame(movie_mdec_state_t* state);
static s32 movie_decode_next_frame(movie_mdec_state_t* state);
static void movie_handle_mdec_slice(void);
static void movie_init_mdec_state(movie_mdec_state_t* state, s32 x0, s32 y0, s32 x1, s32 y1);
static void movie_init_stream(s32 sector, psyq_interrupt_callback_t output_callback);
static void movie_present_frame(void);
static void movie_start_cd_read(const CdlLOC* location);
static void movie_update_stream_frame(void);
static void movie_wait_for_mdec_output(movie_mdec_state_t* state, s32 unused);

/* Each graphics buffer's ordering table holds 16 32-bit entries. */
#define CONTROLLER_OTAG_BUFFER_BYTES 64

/* Entry point of OPEN.BIN for the title sequence. Runs until a controller clears
 * SYSTEM_RUNTIME_FLAG_RUNNING and returns g_title_result: 0 for a new
 * game, or a loaded save with a next scenario and no formation return event,
 * which system_game_loop_run takes straight into BATTLE; 1 for any other
 * loaded save and 2 for the tutorial, which go to WORLD.
 *
 * skip_movie starts at the title menu instead of the FFTST.STR logo movie. Each
 * frame polls the CD read, clears the ordering table and queues the overlay fade,
 * steps the movie, runs the WORLD menu frame while it is on and then the current
 * controller, draws the render records, reads the pad and flips the buffers. */
title_result_e open_title_entrypoint(s32 skip_movie) {
    u32* otag_0;
    u32* otag_1;

    title_sequence_init(skip_movie);
    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_gfx_otags[0];
        otag_1 = g_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            file_load_poll(&g_file_cd_state);
            gfx_clear_and_draw_current_frame();
            movie_update_stream_frame();
            music_test_menu_frame_run();
            controller_dispatch_current();

            graphics_buffer_index = g_gfx_active_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * CONTROLLER_OTAG_BUFFER_BYTES + (u32)otag_0);
            window_dispatch_gpu_record_list(current_otag, g_window_render_object_queue, g_window_render_object_count);

            graphics_buffer_index = g_gfx_active_buffer_index;
            current_otag = (u32*)(graphics_buffer_index * CONTROLLER_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            DrawSync(0);
            input_update_with_soft_reset();
            VSync(0);
            movie_present_frame();
            system_frame_stub();
        } while ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    SetDispMask(0);
    sound_set_cd_mix_and_volume(0xc0, 0x78);
    return g_title_result;
}

/* Entry point of OPEN.BIN for the ending: play FFTEND.STR, then run the ending
 * script until it finishes.
 *
 * Each frame polls the CD read, clears the ordering table, steps the movie,
 * queues the script's primitives, runs the current controller, reads the pad and
 * calls VSync with g_system_vsync_mode (0 during the movie, 2 while the
 * script runs). The ordering table is drawn after the buffer flip, on top of the
 * background movie_present_frame has just put into the new draw buffer. On
 * exit the VRAM page saved by ending_init is restored, the
 * display is blanked and the CD audio returns to mix level 0xc0. */
void open_ending_entrypoint(void) {
    u32* otag_0;
    u32* otag_1;

    ending_init();
    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RUNNING) != 0) {
        otag_0 = g_gfx_otags[0];
        otag_1 = g_gfx_otags[1];
        do {
            s32 graphics_buffer_index;
            u32* current_otag;

            file_load_poll(&g_file_cd_state);
            gfx_clear_and_draw_current_frame();
            movie_update_stream_frame();
            graphics_buffer_index = g_gfx_active_buffer_index;
            /* index * 64 + base: the retail addu sums the offset first. */
            current_otag = (u32*)(graphics_buffer_index * CONTROLLER_OTAG_BUFFER_BYTES + (u32)otag_0);
            ending_draw_text_records(current_otag, graphics_buffer_index);
            controller_dispatch_current();
            DrawSync(0);
            input_update_with_soft_reset();
            VSync(g_system_vsync_mode);
            movie_present_frame();
            current_otag = (u32*)((g_gfx_active_buffer_index ^ 1) * CONTROLLER_OTAG_BUFFER_BYTES + (u32)otag_1);
            DrawOTag(current_otag - 1);
            system_frame_stub();
        } while ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RUNNING) != 0);
    }

    gfx_restore_frame_bin_page();
    SetDispMask(0);
    sound_set_cd_mix_and_volume(0xc0, 0x78);
}

/* Starts streaming a movie. The stream stops with the CD audio paused at end_frame, and
 * without touching the audio at video_end_frame (-1: none). */
void movie_stream_start(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level) {
    sound_set_cd_mix_and_volume(0, 1);
    suzuki_sound_set_type(suzuki_sound_get_type());
    g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | SYSTEM_RUNTIME_FLAG_RGB24;
    gfx_init_screen_environments(1);

    g_movie_aligned_height = 0;
    g_movie_width = 0;
    g_movie_current_frame = 0;
    g_movie_stream_status = MOVIE_STREAM_PLAYING;
    g_movie_end_frame = end_frame;
    g_movie_video_end_frame = video_end_frame;
    movie_init_mdec_state(&g_movie_mdec_stream_state, 0, 8, 0, 0xf8);

    for (;;) {
        movie_init_stream(sector, movie_handle_mdec_slice);
        while (CdRead2(CdlModeStream | CdlModeSpeed | CdlModeRT | CdlModeSize1) == 0) { }
        if (movie_decode_next_frame(&g_movie_mdec_stream_state) != 0) {
            break;
        }
        file_cd_subsystems_reset();
    }

    sound_set_cd_mix_and_volume(cd_mix_level, 0x20);
    PutDrawEnv(&g_gfx_screen_environments[0].draw);
    PutDispEnv(&g_gfx_screen_environments[0].disp);
    g_file_current_openbk_image_id = -1;
    g_system_runtime_flags
        = (g_system_runtime_flags & ~(SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED | SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN))
        | (SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING | SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING);
}

/* Per-frame movie step while a stream plays.
 *
 * When the ring has fewer than 16 free sectors the stream is masked up to the
 * frame StGetBackloc reports and the read restarts at that location. The MDEC is
 * then started on the bitstream decoded last frame and on the first output slice,
 * the next frame's bitstream is decoded, and the routine waits for the picture to
 * be complete. The stream stops when the frame header reached end_frame (pausing
 * the CD audio) or video_end_frame (leaving it playing). */
static void movie_update_stream_frame(void) {
    CdlLOC position;
    s16 free_sectors;
    s16 read_sectors;
    s32 words;

    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        return;
    }

    StRingStatus(&free_sectors, &read_sectors);
    if (free_sectors < 16) {
        StSetMask(1, StGetBackloc(&position), -1);
        movie_start_cd_read(&position);
    }

    DecDCTin(g_movie_mdec_stream_state.vlc_buffers[g_movie_mdec_stream_state.vlc_buffer_index],
        PSYQ_PRESS_MODE_RGB24 | PSYQ_PRESS_MODE_SET_BIT15);
    words = g_movie_mdec_stream_state.output_rect.w;
    words *= g_movie_mdec_stream_state.output_rect.h;
    DecDCTout(g_movie_mdec_stream_state.output_buffers[g_movie_mdec_stream_state.output_buffer_index], words / 2);
    movie_decode_next_frame(&g_movie_mdec_stream_state);
    movie_wait_for_mdec_output(&g_movie_mdec_stream_state, 0);

    if (g_movie_stream_status == MOVIE_STREAM_STOP_AND_PAUSE_AUDIO) {
        movie_stream_stop(1);
    }
    if (g_movie_stream_status == MOVIE_STREAM_STOP) {
        movie_stream_stop(0);
    }
}

/* Stop the movie stream: wait for drawing, remove the MDEC output callback, release the CD ring,
 * pause the CD audio when pause_cd_audio is set, and clear
 * SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING. */
void movie_stream_stop(s32 pause_cd_audio) {
    DrawSync(0);
    DecDCToutCallback(0);
    StUnSetRing();

    if (pause_cd_audio != 0) {
        movie_cd_audio_pause();
    }

    g_system_runtime_flags &= ~SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING;
}

/* While the CD audio plays: drop its volume to 0, pause the drive, wait four
 * vertical blanks and clear SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING. */
void movie_cd_audio_pause(void) {
    if ((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) != 0) {
        sound_set_cd_mix_and_volume(0, 1);

        while (CdControlB(CdlPause, 0, 0) == 0) { }

        VSync(4);
        g_system_runtime_flags ^= SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING;
    }
}

#define MOVIE_MDEC_SLICE_WIDTH 24

/* Sets the two VLC and image buffers and the two frame positions. Retail
 * stores the slice width into the global state, not through `state`. */
static void movie_init_mdec_state(movie_mdec_state_t* state, s32 x0, s32 y0, s32 x1, s32 y1) {
    state->vlc_buffers[0] = (u32*)g_system_work_buffer_0;
    state->vlc_buffers[1] = (u32*)g_system_work_buffer_1;
    state->vlc_buffer_index = 0;
    state->output_buffers[0] = g_movie_mdec_output_buffer_0;
    state->output_buffers[1] = g_movie_mdec_output_buffer_1;
    state->output_buffer_index = 0;
    state->frame_rects[0].x = x0;
    state->frame_rects[0].y = y0;
    state->frame_rects[1].x = x1;
    state->frame_rects[1].y = y1;
    state->frame_rect_index = 0;
    state->output_rect.x = x0;
    state->output_rect.y = y0;
    g_movie_mdec_stream_state.output_rect.w = MOVIE_MDEC_SLICE_WIDTH;
    state->output_ready = 0;
}

/* Prepare the MDEC and the 0x30-sector CD ring for a movie and aim the drive at
 * it: a one-sector read with no destination makes the file loader seek to
 * `sector` and leaves its position in the loader descriptor, which is then
 * set as the read location. */
static void movie_init_stream(s32 sector, psyq_interrupt_callback_t output_callback) {
    DecDCTReset(0);
    DecDCToutCallback(output_callback);
    StSetRing(g_movie_cd_stream_ring_buffer, 0x30);
    StSetStream(1, 0, -1, 0, 0); /* 24-bit mode, from frame 0, no end frame, no callbacks */

    file_read_request_quiet(&g_file_cd_state, sector, 1, 0);
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_load_poll(&g_file_cd_state);
        VSync(0);
    }

    while (CdControl(CdlSetloc, (u8*)&g_file_cd_state.position, 0) == 0) { }
}

/* Uploads the decoded MDEC slice and starts decoding the next one.
 *
 * The rectangle pointer and the output-buffer array are both addressed through
 * the shared stream state, so GCC derives the buffer array (`addiu s1,s0,-0x20`)
 * from the rectangle address. The slice size is built in one variable: its
 * sign extension, product and halving then share the second argument register.
 */
static void movie_handle_mdec_slice(void) {
    RECT* rect;
    s32 size;

    if (g_psyq_cd_stream_interrupt_pending != 0) {
        StCdInterrupt();
        g_psyq_cd_stream_interrupt_pending = 0;
    }
    rect = &g_movie_mdec_stream_state.output_rect;
    LoadImage(rect, g_movie_mdec_stream_state.output_buffers[g_movie_mdec_stream_state.output_buffer_index]);
    g_movie_mdec_stream_state.output_buffer_index = !g_movie_mdec_stream_state.output_buffer_index;
    rect->x += g_movie_mdec_stream_state.output_rect.w;
    if (rect->x < g_movie_mdec_stream_state.frame_rects[g_movie_mdec_stream_state.frame_rect_index].x
            + g_movie_mdec_stream_state.frame_rects[g_movie_mdec_stream_state.frame_rect_index].w) {
        size = g_movie_mdec_stream_state.output_rect.w;
        size *= g_movie_mdec_stream_state.output_rect.h;
        size /= 2;
        DecDCTout(g_movie_mdec_stream_state.output_buffers[g_movie_mdec_stream_state.output_buffer_index], size);
    } else {
        /* The last slice of the picture is uploaded: the next frame decodes into
         * the other frame buffer. */
        g_movie_mdec_stream_state.output_ready = 1;
        g_movie_mdec_stream_state.frame_rect_index = !g_movie_mdec_stream_state.frame_rect_index;
        rect->x = g_movie_mdec_stream_state.frame_rects[g_movie_mdec_stream_state.frame_rect_index].x;
        g_movie_mdec_stream_state.output_rect.y
            = g_movie_mdec_stream_state.frame_rects[g_movie_mdec_stream_state.frame_rect_index].y;
    }
}

/* Fetch the next movie frame and decode its bitstream into the other VLC buffer,
 * releasing the frame's ring sectors. Returns 0 when no frame arrived. */
static s32 movie_decode_next_frame(movie_mdec_state_t* state) {
    u32* frame_data = movie_acquire_next_frame(state);

    if (frame_data == 0) {
        return 0;
    }
    state->vlc_buffer_index = !state->vlc_buffer_index;
    DecDCTvlc(frame_data, state->vlc_buffers[state->vlc_buffer_index]);
    StFreeRing(frame_data);
    return 1;
}

#define MOVIE_ACQUIRE_MAX_ATTEMPTS 0x80

/* Wait for the next movie frame (at most 129 vertical blanks), track the
 * stream position, adopt a changed frame size (clearing the screen), and size
 * the decode rectangles: 24-bit rows take three 16-bit values per two pixels.
 * Returns the frame data, or NULL on timeout. */
static u32* movie_acquire_next_frame(movie_mdec_state_t* state) {
    u32* frame_data;
    StHEADER* header;
    s32 attempts = 0;

    /* Passing header through StGetNext's 32-bit data-pointer slot preserves native header reloads. */
    while (StGetNext(&frame_data, (u32**)&header) != 0) {
        VSync(0);
        if (attempts == MOVIE_ACQUIRE_MAX_ATTEMPTS) {
            return 0;
        }
        attempts++;
    }

    g_movie_current_frame = header->frameCount;
    if (header->frameCount >= g_movie_end_frame) {
        g_movie_stream_status = MOVIE_STREAM_STOP_AND_PAUSE_AUDIO;
    }
    if (g_movie_video_end_frame != -1 && header->frameCount >= g_movie_video_end_frame) {
        g_movie_stream_status = MOVIE_STREAM_STOP;
    }

    if (g_movie_width != header->width || g_movie_height != header->height) {
        RECT clear_rect;

        clear_rect.x = 0;
        clear_rect.y = 0;
        clear_rect.w = 0x1e0;
        clear_rect.h = 0x1e0;
        ClearImage(&clear_rect, 0, 0, 0);
        g_movie_width = header->width;
        g_movie_height = header->height;
        /* The MDEC decodes whole 16x16 macroblocks. */
        g_movie_aligned_height = (header->height + 15) & 0xfff0;
    }

    state->frame_rects[0].w = state->frame_rects[1].w = g_movie_width * 3 / 2;
    state->frame_rects[0].h = state->frame_rects[1].h = g_movie_aligned_height;
    state->output_rect.h = g_movie_aligned_height;
    return frame_data;
}

/* Busy-wait until the MDEC output callback has delivered the whole frame
 * (output_ready), then clear the flag. After 0x800000 passes without it the frame
 * is taken as complete and the output rectangle moves to the other frame buffer.
 * The second argument is not read. */
static void movie_wait_for_mdec_output(movie_mdec_state_t* state, s32 unused) {
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

/* Set the read location and start the streaming read (stream, double speed and
 * 2340-byte sectors), repeating both until the read is accepted. Unlike the first
 * read in movie_stream_start, this mode leaves out CdlModeRT. */
static void movie_start_cd_read(const CdlLOC* location) {
    for (;;) {
        while (CdControl(CdlSetloc, (u8*)location, 0) == 0) { }
        if (CdRead2(CdlModeStream | CdlModeSpeed | CdlModeSize1) != 0) {
            break;
        }
    }
}

/* Flip the frame buffers and repaint the background of the new draw buffer.
 *
 * Applies the other buffer's draw and display environments. Unless a movie is
 * streaming, the 256- or 320-pixel-wide frame is then cleared
 * (SYSTEM_RUNTIME_FLAG_CLEAR_FRAME), overwritten with the 512x240 picture at
 * VRAM (512, 256) (SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM, the ending script)
 * or uploaded from the frame image at g_file_destination. The display is
 * enabled unless SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED is set.
 *
 * Nothing in OPEN.BIN sets SYSTEM_RUNTIME_FLAG_CLEAR_FRAME. */
static void movie_present_frame(void) {
    RECT image_rect;
    RECT move_rect;
    s32 display_index = g_gfx_active_buffer_index ^ 1;
    u32 flags;

    g_gfx_active_buffer_index = display_index;
    PutDrawEnv(&g_gfx_screen_environments[display_index].draw);

    display_index = g_gfx_active_buffer_index;
    PutDispEnv(&g_gfx_screen_environments[display_index].disp);

    flags = g_system_runtime_flags;
    if ((flags & SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        image_rect.h = 240;
        image_rect.x = 0;
        if ((flags & SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) != 0) {
            image_rect.w = 320;
        } else {
            image_rect.w = 256;
        }
        if ((g_gfx_active_buffer_index & 1) != 0) {
            image_rect.y = 240;
        } else {
            image_rect.y = 0;
        }

        flags = g_system_runtime_flags;
        if ((flags & SYSTEM_RUNTIME_FLAG_CLEAR_FRAME) != 0) {
            ClearImage(&image_rect, 0, 0, 0);
        } else if ((flags & SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM) != 0) {
            move_rect.x = 512;
            move_rect.y = 256;
            move_rect.w = 512;
            move_rect.h = 240;
            MoveImage(&move_rect, image_rect.x, image_rect.y);
        } else {
            LoadImage(&image_rect, g_file_destination);
        }
    }

    SetDispMask((g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED) == 0);
}

/* Read the pad and derive the held and newly pressed buttons, both 0 while
 * SYSTEM_RUNTIME_FLAG_INPUT_DISABLED is set. L1, R1, Select and Start held
 * with nothing else reset the game. SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED
 * would prevent that; no code in OPEN.BIN sets it. */
static void input_update_with_soft_reset(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_INPUT_DISABLED) {
        g_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_input_new_button_presses = input & ~previous;
    }

    if (!(g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED)
        && g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        sound_set_cd_mix_and_volume(0, 1);
        system_game_reset();
    }
}

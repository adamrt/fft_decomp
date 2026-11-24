/* Prepare OPEN's shared screen and working state for either session,
 * switching movie display modes and restoring FRAME.BIN after the ending. */

#include "fft/front/ending.h"
#include "fft/front/title.h"
#include "fft/gfx/display_object.h"
#include "fft/gfx/fade.h"
#include "fft/gfx/gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/sys/file.h"
#include "fft/sys/input.h"
#include "fft/sys/movie.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/system.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Draw/display environments rebuilt for title and ending screen modes. */
gfx_screen_environment_t g_gfx_screen_environments[2] = { 0 };

static void init_runtime_state(void);

/* Fades the CD audio (movie streams and XA) in or out over duration ticks.
 * A non-zero cd_mix_level is the CdMix attenuation suzuki_sound_put_type sets
 * for the current sound type, with the SPU's CD volume at 0x6400; 0 fades
 * that volume to 0 and leaves the attenuation alone. */
void sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration) {
    s32 volume;

    if (cd_mix_level == 0) {
        volume = 0;
    } else {
        suzuki_sound_put_type(cd_mix_level);
        volume = 0x6400;
    }

    suzuki_sound_set_cd_volume(volume, duration);
}

/* Set up the title sequence: reset the runtime state, load the title graphics and
 * start the FFTST.STR logo movie, or the title menu when skip_movie is set. */
void title_init_sequence(s32 skip_movie) {
    music_unload_all();
    init_runtime_state();
    title_load_graphics();
    gfx_init_screen_environments(1);
    display_object_init_pools();
    input_init_repeat();
    title_start_sequence(skip_movie);
}

/* Set up the ending: reset the runtime state, park FRAME.BIN's 64x256 VRAM page
 * at (960, 256) in (960, 0), load the ending's graphics and start FFTEND.STR. */
void ending_init(void) {
    RECT source;

    music_unload_all();
    init_runtime_state();

    source.x = 960;
    source.y = 256;
    source.w = 64;
    source.h = 256;
    MoveImage(&source, 0x3c0, 0);
    DrawSync(0);

    g_system_vsync_mode = 0;
    ending_load_graphics();
    gfx_init_screen_environments(1);
    display_object_init_pools();
    movie_start_ending();
}

/* Copy FRAME.BIN's 64x256 VRAM page (the window frames and UI sprites) back
 * from (960, 0), where ending_init parked it before the
 * ending's graphics loaded, to (960, 256). */
void gfx_restore_frame_bin_page(void) {
    RECT source;

    source.x = 960;
    source.y = 0;
    source.w = 64;
    source.h = 256;
    MoveImage(&source, 0x3c0, 0x100);
    DrawSync(0);
}

/* Build the two draw/display environment pairs for the current screen mode.
 *
 * The screen is 320x240 with SYSTEM_RUNTIME_FLAG_WIDE_SCREEN and 256x240
 * without it. Buffer 0 draws at y 0 and displays y 240, buffer 1 the reverse. The
 * drawing offset puts the origin at the center of the screen, the background
 * clear is off, and the overlay fade's rectangle covers the screen in those
 * coordinates. SYSTEM_RUNTIME_FLAG_RGB24 selects the 24-bit display the
 * movies use. Only the 320-pixel branch sets dfe.
 *
 * With clear_first the display is blanked first, the VRAM under both buffers is
 * cleared (half as wide again in 24-bit mode) and the active buffer's
 * environments are applied; without it only the records are filled. */
void gfx_init_screen_environments(s32 clear_first) {
    RECT rect;
    s32 put_envs;
    s16 center_x;

    if (clear_first != 0) {
        SetDispMask(0);
    }

    if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) {
        SetDefDrawEnv(&g_gfx_screen_environments[0].draw, 0, 0, 0x140, 0xf0);
        SetDefDispEnv(&g_gfx_screen_environments[0].disp, 0, 0xf0, 0x140, 0xf0);
        SetDefDrawEnv(&g_gfx_screen_environments[1].draw, 0, 0xf0, 0x140, 0xf0);
        SetDefDispEnv(&g_gfx_screen_environments[1].disp, 0, 0, 0x140, 0xf0);
        center_x = 0xa0;
        g_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_gfx_screen_environments[0].draw.isbg = g_gfx_screen_environments[1].draw.isbg = 0;
        g_gfx_screen_environments[0].draw.dfe = g_gfx_screen_environments[1].draw.dfe = 1;
        g_fade_tile.rect.x = -160;
        g_fade_tile.rect.y = -120;
        g_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_fade_tile.rect.w = 320;
        g_fade_tile.rect.h = 240;
        if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RGB24) {
            g_gfx_screen_environments[0].disp.isrgb24 = g_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 480;
            rect.h = 480;
        } else {
            g_gfx_screen_environments[0].disp.isrgb24 = g_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 320;
            rect.h = 480;
        }
    } else {
        SetDefDrawEnv(&g_gfx_screen_environments[0].draw, 0, 0, 0x100, 0xf0);
        SetDefDispEnv(&g_gfx_screen_environments[0].disp, 0, 0xf0, 0x100, 0xf0);
        SetDefDrawEnv(&g_gfx_screen_environments[1].draw, 0, 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_gfx_screen_environments[1].disp, 0, 0, 0x100, 0xf0);
        center_x = 0x80;
        g_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_gfx_screen_environments[0].draw.isbg = g_gfx_screen_environments[1].draw.isbg = 0;
        g_fade_tile.rect.x = -128;
        g_fade_tile.rect.y = -120;
        g_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_fade_tile.rect.w = 256;
        g_fade_tile.rect.h = 240;
        if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_RGB24) {
            g_gfx_screen_environments[0].disp.isrgb24 = g_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 384;
            rect.h = 480;
        } else {
            g_gfx_screen_environments[0].disp.isrgb24 = g_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 256;
            rect.h = 480;
        }
    }

    if (clear_first != 0) {
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
    }

    /* Retail tests a copy of clear_first here; testing the parameter again
     * compiles differently. */
    put_envs = clear_first;
    if (put_envs != 0) {
        PutDrawEnv(&g_gfx_screen_environments[g_gfx_active_buffer_index].draw);
        PutDispEnv(&g_gfx_screen_environments[g_gfx_active_buffer_index].disp);
        SetGeomOffset(0, 0);
    }
}

/* Reset OPEN.BIN's state on entry: running, display blanked, 320-pixel screen;
 * the frame image buffer at 0x801d0000 and the OPNTEX data buffer at 0x80140000;
 * no OPNBK image read; the overlay fade subtractive in ordering-table entry 2; no
 * buttons, no script and the first title demo movie next. */
static void init_runtime_state(void) {
    g_system_runtime_flags
        = SYSTEM_RUNTIME_FLAG_RUNNING | SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED | SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
    g_file_destination = (u8*)0x801d0000;
    g_file_current_openbk_image_id = -1;
    g_gfx_opntex_data = (u32*)0x80140000;
    g_fade_tile.abr = 2;
    g_fade_tile.ot_index = 2;
    g_input_previous_buttons = 0;
    g_input_new_button_presses = 0;
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = 0;
    g_music_test_menu_otag_index = 4;
    g_ending_script_state.dispatch.flags = 0;
    g_title_demo_movie_index = 0;
}

/* Start a frame: clear the active ordering table and queue the overlay fade. */
void gfx_begin_frame(void) {
    gfx_clear_current_otag();
    fade_tile_update(g_gfx_otags[g_gfx_active_buffer_index]);
}

/* CD mix and volume, opening and ending movie init, frame VRAM page restore, screen environments and runtime state
 * init. */

#include "fft/main_sound.h"
#include "fft/menu_input.h"
#include "fft/open.h"
#include "fft/window.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void open_system_init_runtime_state(void);

/* Fades the CD audio (movie streams and XA) in or out over duration ticks.
 * A non-zero cd_mix_level is the CdMix attenuation suzuki_sound_put_type sets
 * for the current sound type, with the SPU's CD volume at 0x6400; 0 fades
 * that volume to 0 and leaves the attenuation alone. */
void open_sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration) {
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
void open_movie_init_and_stream_opening(s32 skip_movie) {
    main_sound_unload_scenario_music_and_tunes();
    open_system_init_runtime_state();
    open_file_load_main_menu_graphics();
    open_gfx_init_screen_environments(1);
    window_init_record_pools_variant();
    open_input_init_directional_state();
    open_movie_start_fftst_or_title_menu(skip_movie);
}

/* Set up the ending: reset the runtime state, save the 64x256 VRAM page at (960,
 * 256) to (960, 0), load the ending's graphics and start FFTEND.STR. */
void open_movie_init_and_play_end(void) {
    RECT source;

    main_sound_unload_scenario_music_and_tunes();
    open_system_init_runtime_state();

    source.x = 0x3c0;
    source.y = 0x100;
    source.w = 0x40;
    source.h = 0x100;
    MoveImage(&source, 0x3c0, 0);
    DrawSync(0);

    g_open_system_vsync_mode = 0;
    open_file_load_end_movie_graphics();
    open_gfx_init_screen_environments(1);
    window_init_record_pools_variant();
    open_movie_play_end();
}

/* Copy the 64x256 VRAM page kept at (960, 0) back to (960, 256), where
 * open_movie_init_and_play_end saved it from before the ending's picture went
 * over it. */
void open_gfx_restore_frame_vram_page(void) {
    RECT source;

    source.x = 0x3c0;
    source.y = 0;
    source.w = 0x40;
    source.h = 0x100;
    MoveImage(&source, 0x3c0, 0x100);
    DrawSync(0);
}

/* Build the two draw/display environment pairs for the current screen mode.
 *
 * The screen is 320x240 with OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN and 256x240
 * without it. Buffer 0 draws at y 0 and displays y 240, buffer 1 the reverse. The
 * drawing offset puts the origin at the center of the screen, the background
 * clear is off, and the overlay fade's rectangle covers the screen in those
 * coordinates. OPEN_SYSTEM_RUNTIME_FLAG_RGB24 selects the 24-bit display the
 * movies use. Only the 320-pixel branch sets dfe.
 *
 * With clear_first the display is blanked first, the VRAM under both buffers is
 * cleared (half as wide again in 24-bit mode) and the active buffer's
 * environments are applied; without it only the records are filled. */
void open_gfx_init_screen_environments(s32 clear_first) {
    RECT rect;
    s32 put_envs;
    s16 center_x;

    if (clear_first != 0) {
        SetDispMask(0);
    }
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) {
        SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x140, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x140, 0xf0);
        SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x140, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x140, 0xf0);
        center_x = 0xa0;
        g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
        g_open_gfx_screen_environments[0].draw.dfe = g_open_gfx_screen_environments[1].draw.dfe = 1;
        g_open_gfx_overlay_fade.rect.x = -0xa0;
        g_open_gfx_overlay_fade.rect.y = -0x78;
        g_open_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_open_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_open_gfx_overlay_fade.rect.w = 0x140;
        g_open_gfx_overlay_fade.rect.h = 0xf0;
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RGB24) {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x1e0;
            rect.h = 0x1e0;
        } else {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x140;
            rect.h = 0x1e0;
        }
    } else {
        SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x100, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x100, 0xf0);
        SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x100, 0xf0);
        center_x = 0x80;
        g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
        g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
        g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
        g_open_gfx_overlay_fade.rect.x = -0x80;
        g_open_gfx_overlay_fade.rect.y = -0x78;
        g_open_gfx_screen_environments[0].draw.ofs[0] = center_x;
        g_open_gfx_screen_environments[1].draw.ofs[0] = center_x;
        g_open_gfx_overlay_fade.rect.w = 0x100;
        g_open_gfx_overlay_fade.rect.h = 0xf0;
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RGB24) {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 1;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x180;
            rect.h = 0x1e0;
        } else {
            g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0x1e0;
        }
    }
    if (clear_first != 0) {
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
    }
    put_envs = clear_first;
    if (put_envs != 0) {
        PutDrawEnv(&g_open_gfx_screen_environments[g_active_graphics_buffer_index].draw);
        PutDispEnv(&g_open_gfx_screen_environments[g_active_graphics_buffer_index].disp);
        SetGeomOffset(0, 0);
    }
}

/* Reset OPEN.BIN's state on entry: running, display blanked, 320-pixel screen;
 * the frame image buffer at 0x801d0000 and the OPNTEX data buffer at 0x80140000;
 * no OPNBK image read; the overlay fade subtractive in ordering-table entry 2; no
 * buttons, no script and the first title demo movie next. */
static void open_system_init_runtime_state(void) {
    g_open_system_runtime_flags = OPEN_SYSTEM_RUNTIME_FLAG_RUNNING | OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED
        | OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
    g_open_file_destination = (u8*)0x801d0000;
    g_open_file_current_openbk_image_id = -1;
    g_open_gfx_opntex_data = (u32*)0x80140000;
    g_open_gfx_overlay_fade.abr = 2;
    g_open_gfx_overlay_fade.ot_index = 2;
    g_input_previous_buttons = 0;
    g_open_input_new_button_presses = 0;
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = 0;
    g_open_menu_formation_otag_index = 4;
    g_open_script_state.dispatch.flags = 0;
    g_open_title_demo_movie_index = 0;
}

/* Start a frame: clear the active ordering table and queue the overlay fade. */
void open_gfx_clear_and_draw_current_frame(void) {
    open_gfx_clear_current_otag();
    open_gfx_update_fade_overlay(g_open_gfx_otags[g_active_graphics_buffer_index]);
}

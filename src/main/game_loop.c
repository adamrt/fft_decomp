/* Game reset, the BATTLE.BIN entrypoint call, the main game loop, main, the Now Loading message, the display setter,
 * the Squaresoft and SCE boot logos and the display reset. */

#include "fft/battle.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/script_variable.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/libspu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void main_system_stop_display_and_audio_for_game_reset(void) {
    DrawSync(0);
    DrawSync(0);
    SetDispMask(0);
    suzuki_sound_quit();
    SpuQuit();
}

/* Soft reset: stop the display and audio, then restart main_system_run_game_loop
 * on the stack pointer main saved. */
void main_system_reset_game(void) {
    main_system_stop_display_and_audio_for_game_reset();
    main_restore_game_loop_stack_pointer(&g_main_system_game_loop_stack_pointer);
}

/* Call main_run_zero_padding, which only returns (see main_overlay_exec_battle_bin). */
static void main_overlay_call_battle_entrypoint(void) {
    main_run_zero_padding();
}

/* Load BATTLE.BIN (sector 1000) into the whole low overlay area, which ends at
 * the high overlay address, and call main_run_zero_padding. That is not BATTLE
 * code but MAIN's 4 KB of zero words ending in `jr ra`; BATTLE runs when the
 * caller then calls battle_state_run_game_loop. */
static void main_overlay_exec_battle_bin(void) {
    main_file_load_checked_to_address(DISC_SECTOR_BATTLE_BIN,
        g_main_heap_high_overlay_load_address - g_main_heap_low_overlay_load_address,
        g_main_heap_low_overlay_load_address);
    main_overlay_call_battle_entrypoint();
}

/* The top-level loop of the game: opening, then world map and battles until a
 * reset or the ending. It never returns; main_system_reset_game re-enters it from
 * the top with the stack restored.
 *
 * Each pass empties the inventory to the new-game stock and runs OPEN.BIN (after
 * main_boot_reset_game_state, unless the world map asked for the title screen
 * with result 5, which also skips the opening movie). An OPEN.BIN result of 0
 * goes straight into BATTLE.BIN; otherwise WLDCORE and WORLD run first and leave
 * their result in g_main_system_frontend_world_result. World map and BATTLE.BIN
 * then alternate until g_main_system_game_flow_state asks for a reset or the
 * ending. The ending sets event 0x12a, plays OPEN.BIN's ending cutscene and runs
 * BATTLE.BIN once more. */
static void main_system_run_game_loop(void) {
    int open_mode;

    g_main_system_frontend_world_result = 0;

    for (;;) {
        main_item_init_new_game_inventory();

        if (g_main_system_frontend_world_result == 5) {
            open_mode = 1;
        } else {
            main_boot_reset_game_state();
            open_mode = 0;
        }

        if (main_overlay_exec_open_bin_main_loop(open_mode) == 0) {
            g_main_system_go_straight_to_battle = 1;
            g_main_system_frontend_world_result = 0;
        } else {
            g_main_system_go_straight_to_battle = 0;
        }

        do {
            if (g_main_system_go_straight_to_battle == 0) {
                g_main_system_frontend_world_result = main_overlay_open_world_and_wldcore(1);
            }

            if (g_main_system_frontend_world_result == 5) {
                break;
            }

            main_overlay_exec_battle_bin();
            battle_state_run_game_loop();
            main_sound_stop_weather_sfx_music();
            g_main_system_go_straight_to_battle = 0;

            if (g_main_system_game_flow_state == MAIN_GAME_FLOW_RESET) {
                main_system_stop_display_and_audio_for_game_reset();
                break;
            }
        } while (g_main_system_game_flow_state != MAIN_GAME_FLOW_ENDING);

        if (g_main_system_game_flow_state == MAIN_GAME_FLOW_ENDING) {
            main_sound_stop_weather_sfx_music();
            main_sound_unload_scenario_music_and_tunes();
            script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_MALAK_REVIVED);
            main_overlay_exec_open_bin_ending();
            main_overlay_exec_battle_bin();
            main_gfx_load_zodiac_frame();
            battle_state_run_game_loop();
            main_system_stop_display_and_audio_for_game_reset();
        }
    }
}

/* Program entry: run the power-on startup, save the stack pointer for
 * main_system_reset_game and enter the game loop. The loop never returns, so the
 * pad and callback shutdown below it never runs. */
void main(void) {
    main_boot_run_startup();
    main_system_store_stack_pointer(&g_main_system_game_loop_stack_pointer);
    main_system_run_game_loop();
    PadStop();
    StopCallback();
}

/* Build the "Now Loading" message: seven glyph quads from texture page 0x1f with
 * the CLUT loaded to VRAM (0x70, 0x1e2), one copy per display buffer, the text
 * starting at (x + 0xa0, y + 0xc8).
 *
 * visible is stored for main_gfx_add_now_loading_to_otag, which blinks the
 * message, and the blink counter restarts. */
void main_gfx_build_now_loading(u32 visible, s32 x, s32 y) {
    POLY_FT4* prims = g_main_gfx_now_loading_primitives[0];
    POLY_FT4* poly;
    s32 i;

    g_main_gfx_now_loading_visible = visible;
    setPolyFT4(&prims[0]);
    g_main_gfx_now_loading_frame_counter = 0;
    setRGB0(&prims[0], 0x80, 0x80, 0x80);
    SetSemiTrans(&prims[0], 0);
    prims[0].tpage = 0x1f;
    prims[0].clut = getClut(0x70, 0x1e2);
    for (i = 1; i < 7; i++) {
        prims[i] = prims[0];
    }
    /* Dead store, deleted by the compiler: a mention of prims after the copy
     * loop is what makes GCC hoist the block-move end bound (prims + 0x20)
     * out of the loop and allocate prims to s0. */
    poly = prims;

    poly = &g_main_gfx_now_loading_primitives[0][0];
    setXY4(poly, x + 0xA0, y + 0xC8, x + 0xAC, y + 0xC8, x + 0xA0, y + 0xD0, x + 0xAC, y + 0xD0);
    setUV4(poly, 0xE4, 0x9B, 0xF0, 0x9B, 0xE4, 0xA3, 0xF0, 0xA3);
    g_main_gfx_now_loading_primitives[1][0] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][1];
    setXY4(poly, x + 0xAB, y + 0xC8, x + 0xB2, y + 0xC8, x + 0xAB, y + 0xD0, x + 0xB2, y + 0xD0);
    setUV4(poly, 0xE4, 0xA3, 0xEB, 0xA3, 0xE4, 0xAB, 0xEB, 0xAB);
    g_main_gfx_now_loading_primitives[1][1] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][2];
    setXY4(poly, x + 0xB7, y + 0xC8, x + 0xC3, y + 0xC8, x + 0xB7, y + 0xD0, x + 0xC3, y + 0xD0);
    setUV4(poly, 0xAD, 0x55, 0xB9, 0x55, 0xAD, 0x5D, 0xB9, 0x5D);
    g_main_gfx_now_loading_primitives[1][2] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][3];
    setXY4(poly, x + 0xC3, y + 0xC8, x + 0xC8, y + 0xC8, x + 0xC3, y + 0xD0, x + 0xC8, y + 0xD0);
    setUV4(poly, 0xD1, 0x60, 0xD6, 0x60, 0xD1, 0x68, 0xD6, 0x68);
    g_main_gfx_now_loading_primitives[1][3] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][4];
    setXY4(poly, x + 0xC7, y + 0xC8, x + 0xCA, y + 0xC8, x + 0xC7, y + 0xD0, x + 0xCA, y + 0xD0);
    setUV4(poly, 0x7C, 0x52, 0x7F, 0x52, 0x7C, 0x5A, 0x7F, 0x5A);
    g_main_gfx_now_loading_primitives[1][4] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][5];
    setXY4(poly, x + 0xC9, y + 0xC8, x + 0xCE, y + 0xC8, x + 0xC9, y + 0xD0, x + 0xCE, y + 0xD0);
    setUV4(poly, 0xCE, 0xCC, 0xD3, 0xCC, 0xCE, 0xD4, 0xD3, 0xD4);
    g_main_gfx_now_loading_primitives[1][5] = *poly;

    poly = &g_main_gfx_now_loading_primitives[0][6];
    setXY4(poly, x + 0xCD, y + 0xCA, x + 0xD2, y + 0xCA, x + 0xCD, y + 0xD2, x + 0xD2, y + 0xD2);
    setUV4(poly, 0xEB, 0xA4, 0xF0, 0xA4, 0xEB, 0xAC, 0xF0, 0xAC);
    g_main_gfx_now_loading_primitives[1][6] = *poly;

    /* 16-color palette of the NOW LOADING glyph sheet, loaded to VRAM (0x70, 0x1e2). */
    LoadClut2((u32*)g_main_gfx_now_loading_palette, 0x70, 0x1e2);
}

/* Build the Now Loading message centered horizontally (x = 0x80). */
void main_gfx_build_now_loading_center(u32 visible) {
    main_gfx_build_now_loading(visible, 0x80, 0);
}

/* Link the "Now Loading" quads of the current display buffer into otag while the
 * message is visible. It blinks: 60 calls shown, 60 calls hidden. */
void main_gfx_add_now_loading_to_otag(u32* otag) {
    int i;

    if (g_main_gfx_now_loading_visible != 0) {
        if (((g_main_gfx_now_loading_frame_counter / 60) & 1) == 0) {
            for (i = 0; i < 7; i++) {
                AddPrim(otag, &g_main_gfx_now_loading_primitives[g_main_gfx_screen_polarity][i]);
            }
        }
        g_main_gfx_now_loading_frame_counter++;
    }
}

/* Low immediate byte of FntLoad's CLUT-Y addiu instruction. */
extern u8 g_psyq_gpu_font_clut_y_offset_immediate;

/* Install EFC_FNT.TIM as the debug print font: load the built-in font at VRAM (0x280, 0) with its
 * CLUT moved to row 0x7f, overwrite both with EFC_FNT.TIM (retrying until it reads) and open a
 * 512-character print stream at (0x88, 0x10) as the dump font. */
void main_gfx_load_efc_fnt(void) {
    TIM_IMAGE image;
    u32* data;
    int loaded = 0;

    g_psyq_gpu_font_clut_y_offset_immediate = 0x7f;
    FntLoad(0x280, 0);

    while (!loaded) {
        data = main_file_load_checked_to_heap(DISC_SECTOR_BATTLE_EFC_FNT_TIM, 0x8800);
        if (data != 0) {
            if (OpenTIM(data) == 0 && ReadTIM(&image) != 0) {
                LoadTPage(image.paddr, 0, 0, 0x280, 0, image.prect->w * 4, image.prect->h);
                loaded = 1;
                LoadClut2(image.caddr, 0x280, 0x7f);
            }
            main_heap_call_free(data);
        } else {
            VSync(0);
            main_file_poll_load(&g_main_file_cd_state);
        }
    }

    SetDumpFnt(FntOpen(0x88, 0x10, 0x100, 0x100, 0, 0x200));
    DrawSync(0);
    DrawSync(0);
}

/* Set up double-buffered drawing at width x height without restarting the GPU:
 * the second buffer sits 240 lines below the first (a 480-line mode uses one),
 * both draw environments clear to the given color, and the display is masked
 * until the caller turns it on. image_rect is not used. */
void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;

    SetDispMask(0);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 0;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 1;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}

/* Present one frame of the Squaresoft logo fade: swap the display buffers after VSync, upload the
 * 180x16 logo at (70, 112) of the new draw buffer and draw the caller's fade list over it. Returns
 * VSync's result. */
static int main_boot_draw_squaresoft_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);

    destination.x = 70;
    destination.y = g_main_gfx_draw_envs[g_main_gfx_screen_polarity].clip.y + 112;
    destination.w = 180;
    destination.h = 16;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Fade in the Squaresoft logo from the image linked into the executable
 * (g_main_boot_squaresoft_logo_image_words).
 *
 * A full-screen subtractive quad steps from 248 to 0 in 32 frames while
 * main_boot_draw_squaresoft_logo uploads the logo each frame. The logo stays on
 * screen while the caller loads its files; main_boot_fade_out_squaresoft_logo
 * removes it. */
void main_boot_build_and_draw_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    s16 intensity;
    int frame;
    int buffer;

    intensity = 248;
    main_set_display_draw(320, 240, 512, 0, 0, 0);

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (frame = 0; frame < 15; frame++) {
        VSync(0);
    }

    while (intensity >= 0) {
        buffer = g_main_gfx_screen_polarity;
        ClearOTag(otags[buffer], 2);
        /* Reversed indexing preserves the retail compiler's addu operand order. */
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otags[buffer], &polygons[g_main_gfx_screen_polarity]);
        AddPrim(otags[buffer], &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_squaresoft_logo(otags[buffer], g_main_boot_squaresoft_logo_image_words);
        intensity -= 8;
    }

    DrawSync(0);
    DrawSync(0);
}

/* Fade out the Squaresoft logo that main_boot_build_and_draw_squaresoft_logo left
 * on screen: the subtractive quad steps from 0 to 255 in 32 frames, then both
 * display buffers are cleared. */
void main_boot_fade_out_squaresoft_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* otag_base;
    s16 intensity;
    int buffer;

    intensity = 0;
    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 256, 320, 256);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];
    otag_base = otags[0];

    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        buffer = g_main_gfx_screen_polarity;
        ClearOTag(otag_base + buffer * 2, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(otag_base + buffer * 2, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(otag_base + buffer * 2, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_squaresoft_logo(otag_base + buffer * 2, g_main_boot_squaresoft_logo_image_words);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 256;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}

/* Present one frame of the SCEAP.DAT fade: swap the display buffers after VSync,
 * upload the 320x32 strip at line 104 of the new draw buffer and draw the
 * caller's fade list over it. Returns VSync's result. */
int main_boot_draw_sceap_logo(u32* otag, u32* image) {
    RECT destination;
    int elapsed;

    g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
    DrawSync(0);
    elapsed = VSync(0);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);

    destination.x = 0;
    destination.y = g_main_gfx_draw_envs[g_main_gfx_screen_polarity].clip.y + 104;
    destination.w = 320;
    destination.h = 32;
    LoadImage(&destination, image);

    DrawOTag(otag);
    FntFlush(-1);
    return elapsed;
}

/* Show the publisher's logo strip at power-on.
 *
 * Loads SCEAP.DAT (sector 198, a 320x32 16-bit image) to the high overlay address
 * and switches to a 320x240 display. A full-screen subtractive black quad fades
 * the strip in over 32 frames, it stays for 180 frames, and the same quad fades
 * it out over 32; each frame uploads the image again through
 * main_boot_draw_sceap_logo. Both display buffers are cleared before and after. */
void main_boot_build_and_draw_sceap_logo(void) {
    POLY_F4 polygons[2];
    DR_MODE draw_modes[2];
    u32 otags[2][2];
    /* SetDrawMode uses this as a texture window; zero selects the full window. */
    RECT image_rect = { 0 };
    u32* current_otag;
    s16 intensity;
    int frame;

    main_set_display_draw(320, 240, 512, 0, 0, 0);
    main_file_load_checked_to_address(DISC_SECTOR_SCEAP_DAT, 0x5000, g_main_heap_high_overlay_load_address);
    frame = 0;

    SetPolyF4(&polygons[0]);
    setRGB0(&polygons[0], 0, 0, 0);
    setXY4(&polygons[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(&polygons[0], 1);
    polygons[1] = polygons[0];

    SetDrawMode(&draw_modes[0], 0, 0, PSYQ_GPU_TPAGE_ABR_SUBTRACT, &image_rect);
    draw_modes[1] = draw_modes[0];

    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);

    for (; frame < 15; frame++) {
        VSync(0);
    }

    intensity = 248;
    while (intensity >= 0) {
        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_sceap_logo(current_otag, (u32*)g_main_heap_high_overlay_load_address);
        intensity -= 8;
    }

    for (frame = 0; frame < 180; frame++) {
        VSync(0);
    }

    intensity = 0;
    while (intensity < 256) {
        if (intensity >= 248) {
            intensity = 255;
        }

        current_otag = otags[g_main_gfx_screen_polarity];
        ClearOTag(current_otag, 2);
        setRGB0(&g_main_gfx_screen_polarity[polygons], intensity, intensity, intensity);
        AddPrim(current_otag, &polygons[g_main_gfx_screen_polarity]);
        AddPrim(current_otag, &draw_modes[g_main_gfx_screen_polarity]);
        main_boot_draw_sceap_logo(current_otag, (u32*)g_main_heap_high_overlay_load_address);
        intensity += 8;
    }

    DrawSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 320;
    image_rect.h = 480;
    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
}

/* Restart the GPU and set up double-buffered drawing at width x height with the
 * second buffer 240 lines below the first (an interlaced 480-line mode uses one).
 *
 * Unlike main_set_display_draw this also calls ResetGraph(0) and InitGeom, clears
 * the whole 1024x512 VRAM, loads the debug font at (640, 0) and opens a print
 * stream, and leaves background clearing off. The display stays masked. */
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue) {
    RECT image_rect = { 0, 0, 1024, 512 };
    int second_y = height == 480 ? 0 : 240;
    int font_stream;

    SetDispMask(0);
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, second_y, width, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 0;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    ClearImage(&image_rect, 0, 0, 0);
    DrawSync(0);
    FntLoad(640, 0);
    font_stream = FntOpen(40, 16, 512, 256, 0, 512);
    SetDumpFnt(font_stream);

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}

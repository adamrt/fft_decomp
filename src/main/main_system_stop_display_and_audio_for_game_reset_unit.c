#include "fft/battle.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

void main_system_stop_display_and_audio_for_game_reset(void) {
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

void main_overlay_call_battle_entrypoint(void) {
    battle_bin_entrypoint();
}

/* Load BATTLE.BIN (sector 1000) into the whole low overlay area, which ends at
 * the high overlay address, and call its entry point. */
void main_overlay_exec_battle_bin(void) {
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
void main_system_run_game_loop(void) {
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

/* Build the Now Loading message centerd horizontally (x = 0x80). */
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

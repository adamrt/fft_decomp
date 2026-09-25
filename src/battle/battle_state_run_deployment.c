#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Fade in the deployment banner, run map initialisation until it reaches step
 * 0xd, then fade the screen back in.
 *
 * banner_p and the loop's fade = 0xff reproduce the target's preheader order and
 * register choice for the hoisted colour. The dead fade = 0 before done keeps the
 * step-0xd exit inline ahead of the per-frame path, as in the target; with a
 * plain break GCC moves that block to the loop tail. */
void battle_state_run_deployment(void) {
    RECT window = { 0, 0, 0, 0 };
    POLY_FT4 banner[2];
    u8 fade_enabled;
    s32 map_id;
    s16 fade;
    POLY_FT4* banner_p;

    battle_state_init_deployment_display(0x100, 0xf0, 0x200, 0, 0, 0);
    fade_enabled = g_main_system_go_straight_to_battle;
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS) != 0) {
        fade_enabled = 0;
    }
    g_battle_deployment_skipped = battle_menu_init_attack_resources_and_threads();
    main_gfx_build_now_loading_center(g_battle_deployment_skipped);
    map_id = g_battle_map_id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
    g_battle_state_map_init_step = 0;
    g_battle_map_data_load_complete = 0;

    SetPolyF4(&g_battle_screen_fade_polygons[0]);
    g_battle_screen_fade_polygons[0].r0 = 0;
    g_battle_screen_fade_polygons[0].g0 = 0;
    g_battle_screen_fade_polygons[0].b0 = 0;
    g_battle_screen_fade_polygons[0].x0 = 0x80;
    g_battle_screen_fade_polygons[0].y0 = 0;
    g_battle_screen_fade_polygons[0].x1 = 0x180;
    g_battle_screen_fade_polygons[0].y1 = 0;
    g_battle_screen_fade_polygons[0].x2 = 0;
    g_battle_screen_fade_polygons[0].y2 = 0x100;
    g_battle_screen_fade_polygons[0].x3 = 0x180;
    g_battle_screen_fade_polygons[0].y3 = 0x100;
    SetSemiTrans(&g_battle_screen_fade_polygons[0], 1);
    g_battle_screen_fade_polygons[1] = g_battle_screen_fade_polygons[0];
    g_battle_gfx_screen_modulation_polygons[0] = g_battle_screen_fade_polygons[0];
    g_battle_gfx_screen_modulation_polygons[1] = g_battle_screen_fade_polygons[0];
    SetDrawMode(&g_battle_screen_fade_draw_modes[0], 0, 0, 0x40, &window);
    g_battle_screen_fade_draw_modes[1] = g_battle_screen_fade_draw_modes[0];
    g_battle_gfx_screen_modulation_draw_modes[0] = g_battle_screen_fade_draw_modes[0];
    g_battle_gfx_screen_modulation_draw_modes[1] = g_battle_screen_fade_draw_modes[0];

    ((P_TAG*)&banner[0])->len = 9;
    banner[0].code = 0x2c;
    banner[0].r0 = 0x80;
    banner[0].g0 = 0x80;
    banner[0].b0 = 0x80;
    banner[0].x0 = 0x80;
    banner[0].y0 = 0x74;
    banner[0].x1 = 0x17f;
    banner[0].y1 = 0x74;
    banner[0].x2 = 0x80;
    banner[0].y2 = 0x8c;
    banner[0].x3 = 0x17f;
    banner[0].y3 = 0x8c;
    banner[0].u0 = 0;
    banner[0].v0 = 0;
    banner[0].u1 = 0xff;
    banner[0].v1 = 0;
    banner[0].u2 = 0;
    banner[0].v2 = 0x18;
    banner[0].u3 = 0xff;
    banner[0].v3 = 0x18;
    SetSemiTrans(&banner[0], 0);
    banner[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    banner[0].clut = GetClut(0x380, 0x11f);
    banner[1] = banner[0];
    SetDispMask(1);

    fade = 0xf8;
    if (fade_enabled != 0) {
        do {
            main_gfx_swap_and_clear_otag();
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &banner[g_main_gfx_screen_polarity]);
            g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
            g_battle_frame_measurement = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
            fade -= 8;
        } while (fade >= 0);
    }

    banner_p = banner;
    fade = 0xff;
    while (battle_state_update_deployment_controller_input() != -1) {
        main_gfx_swap_and_clear_otag();
        if (fade_enabled != 0) {
            AddPrim(main_gfx_get_otag(), &banner_p[g_main_gfx_screen_polarity]);
        } else {
            main_gfx_add_now_loading_to_otag(main_gfx_get_otag());
        }
        g_battle_state_map_init_step
            = battle_map_init_units_sprites_event_and_music(map_id, g_battle_state_map_init_step,
                battle_script_is_deployment_running(main_gfx_get_otag(), g_controller_input_raw));
        if (g_battle_state_map_init_step == 0xd) {
            battle_gfx_extract_deployed_unit_palettes();
            battle_action_set_casting_unit_id_ff_and_init();
            g_battle_screen_fade_intensity = fade;
            /* Not `break`: jump optimization would move this exit block
             * past the loop end, which the target keeps inline. */
            goto done;
        }
        battle_noop_80079298();
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
        (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
        g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
        g_battle_frame_measurement = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
        main_file_poll_load(&g_main_file_cd_state);
        main_noop_800449ec();
    }
    fade = 0;
done:
    if (fade_enabled != 0) {
        fade = 0;
        do {
            if (fade >= 0xf8) {
                fade = 0xff;
            }
            main_gfx_swap_and_clear_otag();
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->r0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->g0 = fade;
            (g_battle_screen_fade_polygons + g_main_gfx_screen_polarity)->b0 = fade;
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
            AddPrim(main_gfx_get_otag(), &banner[g_main_gfx_screen_polarity]);
            g_main_gfx_screen_polarity = g_main_gfx_screen_polarity == 0;
            g_battle_frame_measurement = battle_state_sync_and_submit_deployment_frame(main_gfx_get_otag() + 0x17f);
            fade += 8;
        } while (fade < 0x100);
    }
}

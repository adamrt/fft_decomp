#include "fft/bunit.h"
#include "fft/main_runtime.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Submit the finished bunit frame and swap to the other graphics context.
 * Twin of card_gfx_submit_frame_and_swap_buffers over the BUNIT globals.
 */
void bunit_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag) {
    u32* previous_otag = g_bunit_gfx_context->otag;
    bunit_gfx_context_t* context;
    /*
     * The target reserves 0x60 bytes of frame it never reads or writes
     * (frame 0x80 with the four register saves at 0x70..0x7c and nothing
     * else sp-relative), so the original source declared a local buffer of
     * this size that the body no longer uses.
     */
    u8 unused_scratch[0x60];

    main_noop_800449ec();
    g_bunit_gfx_poly_f3_count = 0;
    g_bunit_gfx_poly_ft3_count = 0;
    g_bunit_gfx_poly_f4_count = 0;
    g_bunit_gfx_poly_ft4_count = 0;
    g_bunit_gfx_poly_g3_count = 0;
    g_bunit_gfx_poly_gt3_count = 0;
    g_bunit_gfx_poly_g4_count = 0;
    g_bunit_gfx_poly_gt4_count = 0;
    g_bunit_gfx_line_f2_count = 0;
    g_bunit_gfx_line_f3_count = 0;
    g_bunit_gfx_line_f4_count = 0;
    g_bunit_gfx_line_g2_count = 0;
    g_bunit_gfx_line_g3_count = 0;
    g_bunit_gfx_line_g4_count = 0;
    g_bunit_gfx_tile_count = 0;
    g_bunit_gfx_tile_1_count = 0;
    g_bunit_gfx_tile_8_count = 0;
    g_bunit_gfx_tile_16_count = 0;
    g_bunit_gfx_sprite_count = 0;
    g_bunit_gfx_sprite_8_count = 0;
    g_bunit_gfx_sprite_16_count = 0;
    g_bunit_gfx_draw_move_count = 0;
    g_bunit_gfx_draw_area_count = 0;
    g_bunit_gfx_draw_mode_count = 0;
    while (DrawSync(1) != 0) { }
    VSync(g_bunit_menu_event_speed);
    context = g_bunit_gfx_context_base;
    if (g_bunit_gfx_context == context) {
        context++;
    }
    g_bunit_gfx_context = context;
    PutDispEnv(&context->disp_env);
    PutDrawEnv(&g_bunit_gfx_context->draw_env);
    g_bunit_gfx_drawenv_y = g_bunit_gfx_context->draw_env.clip.y;
    if (draw_otag != -1) {
        DrawOTag((u32)(previous_otag + draw_otag));
    }
    ClearOTag(g_bunit_gfx_context->otag + first_otag, g_bunit_gfx_otag_count - first_otag);
}

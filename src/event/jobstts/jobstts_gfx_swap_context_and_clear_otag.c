#include "fft/event_jobstts.h"

/* Reset every primitive count, record the draw environment's y, switch to the
 * other packet buffer and clear its ordering table from entry `first_otag`
 * to the end. The second argument is not read. */
void jobstts_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused) {
    DRAWENV env;
    jobstts_gfx_context_t* context;

    g_gfx_poly_f3_count = 0;
    g_gfx_poly_ft3_count = 0;
    g_jobstts_gfx_poly_f4_count = 0;
    g_menu_gfx_poly_ft4_count = 0;
    g_gfx_poly_g3_count = 0;
    g_gfx_poly_gt3_count = 0;
    g_jobstts_gfx_poly_g4_count = 0;
    g_gfx_poly_gt4_count = 0;
    g_jobstts_gfx_line_f2_count = 0;
    g_gfx_line_f3_count = 0;
    g_gfx_line_f4_count = 0;
    g_gfx_line_g2_count = 0;
    g_gfx_line_g3_count = 0;
    g_gfx_line_g4_count = 0;
    g_jobstts_gfx_tile_count = 0;
    g_gfx_tile_1_count = 0;
    g_gfx_tile_8_count = 0;
    g_gfx_tile_16_count = 0;
    g_gfx_sprite_count = 0;
    g_gfx_sprite_8_count = 0;
    g_gfx_sprite_16_count = 0;
    g_menu_gfx_draw_move_count = 0;
    g_menu_gfx_draw_area_count = 0;
    GetDrawEnv(&env);
    context = g_jobstts_gfx_context_base;
    g_menu_gfx_drawenv_y = env.clip.y;
    if (g_menu_gfx_context == context) {
        context++;
    }
    g_menu_gfx_context = context;
    ClearOTag(&context->otag[first_otag], g_gfx_otag_length - first_otag);
}

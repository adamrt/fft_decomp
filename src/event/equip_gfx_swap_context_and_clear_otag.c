#include "fft/equip.h"
#include "psx/types.h"

/* Reset primitive counts, swap packet buffers, and clear the active OT tail. */
void equip_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused) {
    DRAWENV environment;
    equip_gfx_buffer_t* buffer;

    g_equip_gfx_poly_f3_count = 0;
    g_equip_gfx_poly_ft3_count = 0;
    g_equip_gfx_poly_f4_count = 0;
    g_equip_gfx_poly_ft4_count = 0;
    g_equip_gfx_poly_g3_count = 0;
    g_equip_gfx_poly_gt3_count = 0;
    g_equip_gfx_poly_g4_count = 0;
    g_equip_gfx_poly_gt4_count = 0;
    g_equip_gfx_line_f2_count = 0;
    g_equip_gfx_line_f3_count = 0;
    g_equip_gfx_line_f4_count = 0;
    g_equip_gfx_line_g2_count = 0;
    g_equip_gfx_line_g3_count = 0;
    g_equip_gfx_line_g4_count = 0;
    g_equip_gfx_tile_count = 0;
    g_equip_gfx_tile_1_count = 0;
    g_equip_gfx_tile_8_count = 0;
    g_equip_gfx_tile_16_count = 0;
    g_equip_gfx_sprite_count = 0;
    g_equip_gfx_sprite_8_count = 0;
    g_equip_gfx_sprite_16_count = 0;
    g_equip_gfx_draw_move_count = 0;
    g_equip_gfx_draw_area_count = 0;
    g_equip_gfx_draw_mode_count = 0;
    GetDrawEnv(&environment);
    buffer = g_equip_gfx_context_base;
    g_equip_gfx_drawenv_y = environment.clip.y;
    if (g_equip_gfx_context == &buffer->context) {
        buffer++;
    }
    g_equip_gfx_context = &buffer->context;
    ClearOTag(&buffer->context.otag[first_otag], g_equip_gfx_otag_length - first_otag);
}

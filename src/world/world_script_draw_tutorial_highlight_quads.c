#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Submit the tutorial highlight quads as gradient primitives: the first set
 * fades from a dim left edge to a bright right edge, the second the other
 * way round, with brightness scaled by ten. Built with -O0 like the other
 * tutorial handlers.
 */
void world_script_draw_tutorial_highlight_quads(s32 brightness) {
    s32 i;

    brightness = brightness * 10;
    world_gfx_add_draw_mode_primitive(1, 0, GetTPage(0, 1, 0, 0) & 0xFFFF, 0, 0x3C);
    g_world_tutorial_highlight_colors_a[2][0] = g_world_tutorial_highlight_colors_a[0][0] = brightness >> 1;
    g_world_tutorial_highlight_colors_a[2][1] = g_world_tutorial_highlight_colors_a[0][1] = brightness >> 3;
    g_world_tutorial_highlight_colors_a[2][2] = g_world_tutorial_highlight_colors_a[0][2] = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1][0] = g_world_tutorial_highlight_colors_b[3][0] = brightness >> 1;
    g_world_tutorial_highlight_colors_b[1][1] = g_world_tutorial_highlight_colors_b[3][1] = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1][2] = g_world_tutorial_highlight_colors_b[3][2] = brightness >> 3;
    g_world_tutorial_highlight_colors_a[3][0] = g_world_tutorial_highlight_colors_a[1][0] = brightness;
    g_world_tutorial_highlight_colors_a[3][1] = g_world_tutorial_highlight_colors_a[1][1] = brightness >> 2;
    g_world_tutorial_highlight_colors_a[3][2] = g_world_tutorial_highlight_colors_a[1][2] = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0][0] = g_world_tutorial_highlight_colors_b[2][0] = brightness;
    g_world_tutorial_highlight_colors_b[0][1] = g_world_tutorial_highlight_colors_b[2][1] = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0][2] = g_world_tutorial_highlight_colors_b[2][2] = brightness >> 2;
    for (i = 0; i < g_world_tutorial_highlight_quad_count; i++) {
        world_gfx_append_poly_g4_to_otag(
            g_world_tutorial_highlight_corners_a[i], g_world_tutorial_highlight_colors_a[0], 1, 0x3D);
        world_gfx_append_poly_g4_to_otag(
            g_world_tutorial_highlight_corners_b[i], g_world_tutorial_highlight_colors_b[0], 1, 0x3D);
    }
}

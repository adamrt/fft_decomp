#include "fft/world.h"
#include "psx/libgpu.h"
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
    menu_gfx_enqueue_draw_mode(1, 0, GetTPage(0, 1, 0, 0), 0, 0x3C);
    g_world_tutorial_highlight_colors_a[2].r = g_world_tutorial_highlight_colors_a[0].r = brightness >> 1;
    g_world_tutorial_highlight_colors_a[2].g = g_world_tutorial_highlight_colors_a[0].g = brightness >> 3;
    g_world_tutorial_highlight_colors_a[2].b = g_world_tutorial_highlight_colors_a[0].b = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1].r = g_world_tutorial_highlight_colors_b[3].r = brightness >> 1;
    g_world_tutorial_highlight_colors_b[1].g = g_world_tutorial_highlight_colors_b[3].g = brightness >> 3;
    g_world_tutorial_highlight_colors_b[1].b = g_world_tutorial_highlight_colors_b[3].b = brightness >> 3;
    g_world_tutorial_highlight_colors_a[3].r = g_world_tutorial_highlight_colors_a[1].r = brightness;
    g_world_tutorial_highlight_colors_a[3].g = g_world_tutorial_highlight_colors_a[1].g = brightness >> 2;
    g_world_tutorial_highlight_colors_a[3].b = g_world_tutorial_highlight_colors_a[1].b = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0].r = g_world_tutorial_highlight_colors_b[2].r = brightness;
    g_world_tutorial_highlight_colors_b[0].g = g_world_tutorial_highlight_colors_b[2].g = brightness >> 2;
    g_world_tutorial_highlight_colors_b[0].b = g_world_tutorial_highlight_colors_b[2].b = brightness >> 2;
    for (i = 0; i < g_world_tutorial_highlight_quad_count; i++) {
        world_gfx_enqueue_gouraud_quad(
            g_world_tutorial_highlight_corners_a[i], g_world_tutorial_highlight_colors_a, 1, 0x3D);
        world_gfx_enqueue_gouraud_quad(
            g_world_tutorial_highlight_corners_b[i], g_world_tutorial_highlight_colors_b, 1, 0x3D);
    }
}

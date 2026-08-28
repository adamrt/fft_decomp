#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/libc.h"

/* Flat-textured triangle; the psx/gpu.h clean-room set does not declare it. */

/* Set up the weather particles and their flat-textured sprites.
 *
 * 0x8f (snow, from battle_map_set_weather_texture_overlay 0x53/0x7e/0x7f) scatters 64 flakes;
 * 0x90 (rain, 0x55/0x7c/0x7d) scatters 32 drop pairs over the map extent and
 * records the lowest tile surface (largest y) in g_battle_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
void battle_map_init_weather_particles(s32 command) {
    battle_weather_sprite_rects_t rects;
    s16 sprite;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    rects = g_battle_map_weather_drop_uv_rects;
    switch (command & 0xff) {
    case 0x8f:
        g_battle_map_weather_effect_mode = 0x53;
        for (i = 0; i < 64; i++) {
            g_battle_map_weather_particles[i].y = -rand() % 240 - 16;
            g_battle_map_weather_particles[i].fall_speed = g_battle_map_weather_fall_speed;
            sprite = rand() % 3;
            for (j = 0; j < 2; j++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 0);
                g_battle_render_buffers[j].ft3[i].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, 0x1e2);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i].u0 = rects.rects[sprite].x;
                g_battle_render_buffers[j].ft3[i].u1 = rects.rects[sprite].x + rects.rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].u2 = rects.rects[sprite].x + rects.rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].v0 = rects.rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v1 = rects.rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v2 = rects.rects[sprite].y + rects.rects[sprite].h;
            }
            g_battle_map_weather_particles[i].fall_speed = sprite + g_battle_map_weather_fall_speed;
            if (rand() & 1) {
                g_battle_map_weather_particles[i].x = rand() % 140;
            } else {
                g_battle_map_weather_particles[i].x = -(rand() % 140);
            }
            if (rand() & 1) {
                g_battle_map_weather_particles[i].z = rand() % 140;
            } else {
                g_battle_map_weather_particles[i].z = -(rand() % 140);
            }
        }
        break;
    case 0x90:
        g_battle_map_weather_effect_mode = 0x55;
        for (i = 0; i < 32; i++) {
            g_battle_map_weather_particles[i].x = rand() % (g_battle_map_tile_width * 28);
            g_battle_map_weather_particles[i].z = rand() % (g_battle_map_tile_depth * 28);
            g_battle_map_weather_particles[i].y = -(rand() % 384) - 384;
            g_battle_map_weather_particles[i + 32].x = g_battle_map_weather_particles[i].x;
            g_battle_map_weather_particles[i + 32].z = g_battle_map_weather_particles[i].z;
            g_battle_map_weather_particles[i + 32].y = g_battle_map_weather_particles[i].y - rand() % 48;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < 32; i++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 1);
                g_battle_render_buffers[j].ft3[i].r0 = 0x64;
                g_battle_render_buffers[j].ft3[i].g0 = 0x64;
                g_battle_render_buffers[j].ft3[i].b0 = 0x64;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, 0x1e2);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                prim = (POLY_FT3*)((u8*)g_battle_render_buffers[0].ft3
                    + (j * sizeof(battle_render_buffer_t) + i * sizeof(POLY_FT3)));
                prim->u0 = 0xd8;
                prim->v0 = 0xaf;
                prim->u1 = 0xf7;
                prim->v1 = 0xaf;
                prim->u2 = 0xd8;
                prim->v2 = 0xaf;
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i + 32], 1);
                g_battle_render_buffers[j].ft3[i + 32].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].clut = GetClut(0, 0x1e2);
                g_battle_render_buffers[j].ft3[i + 32].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i + 32].u0 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].u1 = 0x1f;
                g_battle_render_buffers[j].ft3[i + 32].v0 = 0xa0;
                g_battle_render_buffers[j].ft3[i + 32].u2 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].v1 = 0xaf;
                g_battle_render_buffers[j].ft3[i + 32].v2 = 0xaf;
            }
        }
        g_battle_map_lowest_surface_y = 0x80000000;
        g_battle_map_tile_data_ptr = g_battle_map_tile_data;
        for (i = 0; i < g_battle_map_tile_width; i++) {
            for (j = 0; j < g_battle_map_tile_depth; j++) {
                surface_y = -(
                    (g_battle_map_tile_data_ptr[(s16)(i + j * g_battle_map_tile_width)].height
                        + (g_battle_map_tile_data_ptr[(s16)(i + j * g_battle_map_tile_width)].depth_half_height >> 5))
                    * 12);
                if (g_battle_map_lowest_surface_y < surface_y) {
                    g_battle_map_lowest_surface_y = surface_y;
                }
            }
        }
        break;
    }
}

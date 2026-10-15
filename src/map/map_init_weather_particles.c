#include "fft/battle_map.h"
#include "psx/libgpu.h"

/* Flat-textured triangle; the psx/libgpu.h clean-room set does not declare it. */

/* Set up the weather particles and their flat-textured sprites.
 *
 * 0x8f (snow, from map_run_weather_command 0x53/0x7e/0x7f) scatters 64 flakes;
 * 0x90 (rain, 0x55/0x7c/0x7d) scatters 32 drop pairs over the map extent and
 * records the lowest tile surface (largest y) in g_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
void map_init_weather_particles(map_weather_command_e command) {
    RECT rects[3];
    s16 sprite;
    s16 tile;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    memcpy(rects, g_map_weather_drop_uv_rects, sizeof(rects));
    switch (command & 0xff) {
    case MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_SNOW;
        for (i = 0; i < 64; i++) {
            g_map_weather_particles[i].y = -rand() % 240 - 16;
            g_map_weather_particles[i].fall_speed = g_map_weather_fall_speed;
            sprite = rand() % 3;
            for (j = 0; j < 2; j++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 0);
                g_battle_render_buffers[j].ft3[i].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i].u0 = rects[sprite].x;
                g_battle_render_buffers[j].ft3[i].u1 = rects[sprite].x + rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].u2 = rects[sprite].x + rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].v0 = rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v1 = rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v2 = rects[sprite].y + rects[sprite].h;
            }
            g_map_weather_particles[i].fall_speed = sprite + g_map_weather_fall_speed;
            if (rand() & 1) {
                g_map_weather_particles[i].x = rand() % 140;
            } else {
                g_map_weather_particles[i].x = -(rand() % 140);
            }
            if (rand() & 1) {
                g_map_weather_particles[i].z = rand() % 140;
            } else {
                g_map_weather_particles[i].z = -(rand() % 140);
            }
        }
        break;
    case MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_RAIN;
        for (i = 0; i < 32; i++) {
            g_map_weather_particles[i].x = rand() % (g_map_tile_width * MAP_TILE_SIZE);
            g_map_weather_particles[i].z = rand() % (g_map_tile_depth * MAP_TILE_SIZE);
            g_map_weather_particles[i].y = -(rand() % 384) - 384;
            g_map_weather_particles[i + 32].x = g_map_weather_particles[i].x;
            g_map_weather_particles[i + 32].z = g_map_weather_particles[i].z;
            g_map_weather_particles[i + 32].y = g_map_weather_particles[i].y - rand() % 48;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < 32; i++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 1);
                g_battle_render_buffers[j].ft3[i].r0 = 0x64;
                g_battle_render_buffers[j].ft3[i].g0 = 0x64;
                g_battle_render_buffers[j].ft3[i].b0 = 0x64;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                prim = (POLY_FT3*)((u8*)g_battle_render_buffers[0].ft3
                    + (j * sizeof(battle_render_buffer_t) + i * sizeof(POLY_FT3)));
                setUV3(prim, 0xd8, 0xaf, 0xf7, 0xaf, 0xd8, 0xaf);
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i + 32], 1);
                g_battle_render_buffers[j].ft3[i + 32].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i + 32].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i + 32].u0 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].u1 = 0x1f;
                g_battle_render_buffers[j].ft3[i + 32].v0 = 0xa0;
                g_battle_render_buffers[j].ft3[i + 32].u2 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].v1 = 0xaf;
                g_battle_render_buffers[j].ft3[i + 32].v2 = 0xaf;
            }
        }
        g_map_lowest_surface_y = 0x80000000;
        g_map_tile_data_ptr = g_map_tile_data;
        for (i = 0; i < g_map_tile_width; i++) {
            for (j = 0; j < g_map_tile_depth; j++) {
                tile = i + j * g_map_tile_width;
                surface_y
                    = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
                if (g_map_lowest_surface_y < surface_y) {
                    g_map_lowest_surface_y = surface_y;
                }
            }
        }
        break;
    }
}

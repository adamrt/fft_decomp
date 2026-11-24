/* Cycle the tile-glow palettes and project the map cursor and Wait direction indicator. */

#include "fft/battle/camera.h"
#include "fft/battle/cursor.h"
#include "fft/game/battle_unit.h"
#include "fft/gfx/gfx.h"
#include "fft/map/map.h"
#include "fft/map/map_palette.h"
#include "fft/map/map_unit.h"
#include "fft/sys/math.h"
#include "fft/sys/system.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Palette cycling for the cursor tile glow. */
extern palette_t g_cursor_tile_glow_cycled_palettes[2];
static s32 s_cursor_tile_glow_cycle_offset;
static s32 s_cursor_tile_glow_cycle_timer;

/* Cycles the colors of the two cursor tile glow palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
static void update_tile_glow_palettes(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    s_cursor_tile_glow_cycle_timer += g_system_vsync_interval;
    if (s_cursor_tile_glow_cycle_timer >= 4) {
        s_cursor_tile_glow_cycle_timer = 0;
        s_cursor_tile_glow_cycle_offset++;
        if (s_cursor_tile_glow_cycle_offset >= 15) {
            s_cursor_tile_glow_cycle_offset = 0;
        }
        if (s_cursor_tile_glow_cycle_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = s_cursor_tile_glow_cycle_offset * 2 + 2;
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[0] = ((u8*)g_cursor_tile_glow_palettes)[0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[1] = ((u8*)g_cursor_tile_glow_palettes)[1];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[0] = *(u16*)&g_cursor_tile_glow_palettes[1][0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[1] = ((u8*)g_cursor_tile_glow_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            for (src = 2, dst = 32 - s_cursor_tile_glow_cycle_offset * 2; src < s_cursor_tile_glow_cycle_offset * 2 + 2;
                src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            palettes = g_cursor_tile_glow_cycled_palettes;
        } else {
            palettes = g_cursor_tile_glow_palettes;
        }
        map_palette_load_data(palettes, MAP_CLUT_ROW_SPRITES, 1, 0);
        map_palette_load_data(palettes + 1, MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}

/* Project the center of the cursor tile, at its surface height less any water depth, to the screen
 * and store the cursor's screen x and y. `position` receives the tile point and `raised` its depth
 * point. It first cycles the tile glow palettes. */
void cursor_project_tile(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    map_tile_t* tile;
    s32 flag;

    update_tile_glow_palettes();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_set_short(position, g_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
        g_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    position->vy = map_calculate_slope_height(position, (u8)g_cursor_layer);
    tile = map_get_tile((position->vx / MAP_TILE_SIZE), (position->vz / MAP_TILE_SIZE), g_cursor_layer);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_get_biased_tile_depth_point(position, raised);
    } else {
        camera_get_unbiased_tile_depth_point(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_cursor_screen_x = projected->vx;
    g_cursor_screen_y = projected->vy;
}

/* Project the cursor tile and draw the map cursor over it with the glow quad on the tile surface;
 * update_cursor calls it while g_cursor_visible is set. The tile's
 * untargetable flag picks the glow's palette. */
void cursor_draw_tile_glow(void) {
    SVECTOR position;
    SVECTOR raised;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR projected;
    s32 depth;
    s32 shape;
    map_tile_t* tile;

    cursor_project_tile(&projected, &position, &raised);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = projected.vz / 4;
    cursor_draw_map_selection(&position, gfx_get_battle_otag());
    /* The polygon routine reads the same three 16-bit values in renderer x, z, y order. */
    shape = cursor_calculate_tile_polygon(&position, g_cursor_layer, &corner0);
    tile = map_get_tile(position.vx / MAP_TILE_SIZE, position.vz / MAP_TILE_SIZE, g_cursor_layer);
    cursor_build_tile_glow(
        tile->flags.untargetable, shape, &corner0, &corner1, &corner2, &corner3, gfx_get_battle_otag() + depth);
}

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void map_unit_facing_draw_wait_direction(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    s32 flag;
    map_unit_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    unit = map_unit_get_source();
    vector_set_short(&base, unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    /* The second RotTrans overwrites this result unread. */
    RotTrans(&base, &transformed, &flag);
    camera_get_unbiased_tile_depth_point(&unit->world_coords, &raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = gfx_get_battle_otag();
    map_unit_facing_draw_wait_arrows(&base, &ot[depth]);
}

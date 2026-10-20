/* Cycle the tile-glow palettes and project the map cursor and Wait direction indicator. */

#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/gfx.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Palette cycling for the cursor tile glow. */
extern palette_t g_cursor_tile_glow_cycled_palettes[2];
extern s32 g_cursor_tile_glow_cycle_offset;
extern s32 g_cursor_tile_glow_cycle_timer;

/* Cycles the colors of the two cursor tile glow palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
void cursor_tile_glow_palettes_update(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    g_cursor_tile_glow_cycle_timer += g_system_vsync_interval;
    if (g_cursor_tile_glow_cycle_timer >= 4) {
        g_cursor_tile_glow_cycle_timer = 0;
        g_cursor_tile_glow_cycle_offset++;
        if (g_cursor_tile_glow_cycle_offset >= 15) {
            g_cursor_tile_glow_cycle_offset = 0;
        }
        if (g_cursor_tile_glow_cycle_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_cursor_tile_glow_cycle_offset * 2 + 2;
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[0] = ((u8*)g_cursor_tile_glow_palettes)[0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[1] = ((u8*)g_cursor_tile_glow_palettes)[1];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[0] = *(u16*)&g_cursor_tile_glow_palettes[1][0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[1] = ((u8*)g_cursor_tile_glow_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_cursor_tile_glow_cycle_offset * 2; src < g_cursor_tile_glow_cycle_offset * 2 + 2;
                src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            palettes = g_cursor_tile_glow_cycled_palettes;
        } else {
            palettes = g_cursor_tile_glow_palettes;
        }
        battle_map_palette_load_data(palettes, BATTLE_MAP_CLUT_ROW_SPRITES, 1, 0);
        battle_map_palette_load_data(palettes + 1, BATTLE_MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}

/* Project the center of the cursor tile, at its surface height less any water depth, to the screen
 * and store the cursor's screen x and y. `position` receives the tile point and `raised` its depth
 * point. It first cycles the tile glow palettes. */
void cursor_tile_project(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    battle_map_tile_t* tile;
    s32 flag;

    cursor_tile_glow_palettes_update();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_short_set(position, g_cursor_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), 0,
        g_cursor_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
    position->vy = battle_map_slope_height_calculate(position, (u8)g_cursor_layer);
    tile = battle_map_tile_get_pointer(
        (position->vx / BATTLE_MAP_TILE_SIZE), (position->vz / BATTLE_MAP_TILE_SIZE), g_cursor_layer);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_get_tile_depth_point_biased(position, raised);
    } else {
        camera_get_tile_depth_point_unbiased(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_cursor_screen_x = projected->vx;
    g_cursor_screen_y = projected->vy;
}

/* Project the cursor tile and draw the map cursor over it with the glow quad on the tile surface;
 * cursor_update calls it while g_cursor_visible is set. The tile's
 * untargetable flag picks the glow's palette. */
void cursor_tile_glow_draw(void) {
    SVECTOR position;
    SVECTOR raised;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR projected;
    s32 depth;
    s32 shape;
    battle_map_tile_t* tile;

    cursor_tile_project(&projected, &position, &raised);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = projected.vz / 4;
    cursor_map_selection_draw(&position, gfx_battle_otag_get());
    /* The polygon routine reads the same three 16-bit values in renderer x, z, y order. */
    shape = cursor_calculate_tile_polygon(&position, g_cursor_layer, &corner0);
    tile = battle_map_tile_get_pointer(
        position.vx / BATTLE_MAP_TILE_SIZE, position.vz / BATTLE_MAP_TILE_SIZE, g_cursor_layer);
    cursor_tile_glow_build(
        tile->flags.untargetable, shape, &corner0, &corner1, &corner2, &corner3, gfx_battle_otag_get() + depth);
}

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void unit_facing_draw_wait_direction(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    s32 flag;
    unit_runtime_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    unit = unit_source_get();
    vector_short_set(&base, unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    /* The second RotTrans overwrites this result unread. */
    RotTrans(&base, &transformed, &flag);
    camera_get_tile_depth_point_unbiased(&unit->world_coords, &raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = gfx_battle_otag_get();
    unit_facing_draw_wait_arrows(&base, &ot[depth]);
}

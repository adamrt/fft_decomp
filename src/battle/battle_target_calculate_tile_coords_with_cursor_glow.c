#include "fft/battle.h"
#include "psx/types.h"

/*
 * Tile coordinate calculation with the cursor glow, used while an AT list is
 * active (see battle_target_calculate_tile_coords_and_glow_from_at_list).
 */
void battle_target_calculate_tile_coords_with_cursor_glow(void) {
    SVECTOR secondary;
    SVECTOR tertiary;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR primary;
    s32 depth;
    s32 result;
    map_tile_t* tile;

    battle_target_project_cursor_tile_to_screen(&primary, &secondary, &tertiary);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = primary.vz / 4;
    battle_gfx_draw_map_selection_cursor(&secondary, main_gfx_get_otag());
    /* The polygon routine reads the same three halfwords in renderer x, z, y order. */
    result
        = battle_target_calculate_cursor_tile_polygon((battle_screen_coords_t*)&secondary, g_battle_cursor_z, &corner0);
    tile = battle_map_get_tile_data_pointer(secondary.vx / 28, secondary.vz / 28, g_battle_cursor_z);
    battle_gfx_build_cursor_tile_glow(
        tile->flags_06.bits.untargetable, result, &corner0, &corner1, &corner2, &corner3, main_gfx_get_otag() + depth);
}

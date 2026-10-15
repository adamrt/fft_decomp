#include "fft/battle.h"
#include "fft/battle_unit.h"

/* Face a renderer unit toward the current cursor tile.
 *
 * The 14-pixel adjustment compares the projected unit position with the tile
 * center; masking the fixed-point angle selects one of four cardinal facings. */
void battle_unit_face_toward_cursor(battle_unit_t* unit) {
    VECTOR difference;
    s32 unit_x;
    s32 unit_y;

    if (unit != 0) {
        s32 cursor_x = g_battle_cursor_x;
        s32 cursor_y = g_battle_cursor_y;
        s32 tile_x;
        s32 tile_y;
        difference.vz = 0;
        difference.vx = 0;
        tile_x = cursor_x * MAP_TILE_SIZE;
        unit_x = unit->world_coords.vx - MAP_TILE_SIZE / 2;
        difference.vx = tile_x - unit_x;
        tile_y = cursor_y * MAP_TILE_SIZE;
        unit_y = unit->world_coords.vz - MAP_TILE_SIZE / 2;
        difference.vz = tile_y - unit_y;
        if (difference.vx != 0 || difference.vz != 0) {
            unit->facing = (ratan2(-difference.vz, difference.vx) - 0x200) & 0xC00;
        }
    }
}

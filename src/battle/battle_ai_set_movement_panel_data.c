#include "fft/battle.h"
#include "psx/etc.h"

/* Local view of battle_ai_data_t::walkable_tiles as level/y rows. */
typedef struct battle_ai_walkable_view {
    u8 _unused_000[0xc24];
    u16 walkable_tiles[2][18];
} battle_ai_walkable_view_t;

/* Build the AI walkable and reachable tile rows for the acting unit.
 *
 * A fresh decision (decision_state 0) sets the water penalty, rebuilds the
 * walkable rows, clears the scenario rows and spreads movement unless the
 * unit has moved or cannot move. The reachable rows then drop occupied tiles
 * and, unless the unit is disabled, the four listed coordinates. Returns -1
 * when the frame budget is spent. i and j are reused as the spread unit id
 * and the teleport-range distances, as the target's register assignment
 * shows. */
s32 battle_ai_set_movement_panel_data(s32 movement_taken) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    map_tile_t* tile;
    s32 level;
    s32 y;
    s32 x;
    s32 i;
    u8 mount_info;
    s32 j;
    battle_ai_coords_t* coords;

    if (g_battle_ai_data_base.decision_state == 0) {
        if ((g_battle_ai_data_base.acting_unit->movement_abilities[2] & 0xc8)
            || (g_battle_ai_data_base.acting_unit->status_sets.current[2] & 0x40)) {
            g_battle_ai_data_base.water_penalty = 0;
        } else {
            g_battle_ai_data_base.water_penalty = 1;
        }
        battle_ai_clear_words((s32*)((battle_ai_walkable_view_t*)ai)->walkable_tiles, 0x48);
        for (level = 0; level < 2; level++) {
            for (y = 0; y < ai->map_max_y; y++) {
                for (x = 0; x < ai->map_max_x; x++) {
                    tile = &g_battle_map_tile_data[level * 256 + y * ai->map_max_x + x];
                    if (!(tile->flags_06.value & 1) && (tile->surface.value & 0x3f) != 0x3f) {
                        ((battle_ai_walkable_view_t*)ai)->walkable_tiles[level][y] |= 0x8000 >> x;
                    }
                }
            }
        }
        ai->movement_scenario = 0;
        for (j = 0; j < 18; j++) {
            ai->reachable_tiles[ai->movement_scenario][0][j] = 0;
            ai->reachable_tiles[ai->movement_scenario][1][j] = 0;
        }
        if (movement_taken != 0 || (ai->acting_unit->status_sets.current[4] & 0x08)) {
            ai->reachable_tiles[0][ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
                = 0x8000 >> ai->acting_unit_coords.bytes.x;
            return 0;
        }
        mount_info = ai->acting_unit->mount_info;
        if (mount_info & 0x80) {
            i = mount_info & 0x1f;
        } else {
            i = ai->acting_unit_id;
        }
        battle_move_set_reachable_tiles(
            i, ai->acting_unit_coords.bytes.x, ai->acting_unit_coords.bytes.y, ai->acting_unit_coords.bytes.elevation);
    }
    if (VSync(1) > 0x1b8) {
        return -1;
    }
    ai->decision_state = 0;
    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            for (x = 0; x < ai->map_max_x; x++) {
                tile = &g_battle_map_tile_data[level * 256 + y * ai->map_max_x + x];
                if (ai->acting_unit->movement_abilities[1] & 0x08) {
                    if (!(ai->acting_unit->mount_info & 0x80)) {
                        i = ai->acting_unit_coords.bytes.x - x;
                        if (i < 0) {
                            i = -i;
                        }
                        j = ai->acting_unit_coords.bytes.y - y;
                        if (j < 0) {
                            j = -j;
                        }
                        if (ai->acting_unit_move < i + j) {
                            continue;
                        }
                    }
                }
                if (tile->ceiling_depth_and_marks & 0x20) {
                    ai->reachable_tiles[0][level][y] |= 0x8000 >> x;
                }
            }
        }
    }
    for (i = 0; i < 21; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && i != ai->acting_unit_id
            && !(*(u16*)&unit->status_sets.current[0] & 0x140)) {
            ai->reachable_tiles[0][unit->position.bits.higher_elevation][unit->position.bits.y] &= ~(0x8000 >> unit->x);
        }
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
        |= 0x8000 >> ai->acting_unit_coords.bytes.x;
    if (*(u32*)&ai->acting_unit->status_sets.current[0] & 0x81000) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        coords = &ai->coords_181c[i];
        if (coords->bytes.x == 0xff) {
            break;
        }
        ai->reachable_tiles[0][coords->bytes.elevation][coords->bytes.y] &= ~(0x8000 >> coords->bytes.x);
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.bytes.elevation][ai->acting_unit_coords.bytes.y]
        |= 0x8000 >> ai->acting_unit_coords.bytes.x;
    return 0;
}

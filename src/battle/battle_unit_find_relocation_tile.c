#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/map.h"
#include "psx/types.h"

/* Find a free tile near a unit for it to be moved to, returning 0 or -1.
 *
 * Panels holding another unit or an earlier destination are marked in
 * unit_record_index. Rings of increasing Manhattan radius around the unit are then
 * searched, first up to radius 3 within -10..+4 of the unit's height, then up
 * to radius 16 at any height, for a flat enough, unblocked tile that is not
 * lava or an obstacle. The chosen tile is also appended to the destination
 * list.
 *
 * The panel clear walks the byte offset itself: the target loop has no index
 * register, which an index loop keeps. Only level's destination store is an
 * assignment chain; that shape alone gives the target's register choices. */
s32 battle_unit_find_relocation_tile(s32 unit_index, battle_dismount_coords_t* destination) {
    battle_stats_t* unit;
    map_tile_t* tile;
    s32 origin_x;
    s32 origin_y;
    s32 low;
    s32 high;
    s32 i;
    s32 n;
    s32 height;
    s32 pass;
    s32 limit;
    s32 dx;
    s32 dy;
    s32 x;
    s32 y;
    s32 side;
    s32 level;
    s32 index;
    s32 surface;
    u32 depth;
    s32 flags;
    s32 found_x;
    s32 found_y;
    s32 total;
    battle_target_panel_t* panel;

    unit = &g_battle_unit_stats[unit_index];
    origin_x = unit->x;
    origin_y = unit->position.bits.y;
    for (n = 0x1ff * sizeof(battle_target_panel_t); n >= 0; n -= sizeof(battle_target_panel_t)) {
        ((battle_target_panel_t*)((u8*)g_battle_target_panels + n))->unit_record_index = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            index = battle_map_calculate_location(unit);
            panel = &g_battle_target_panels[index];
            tile = &g_battle_map_tile_data[index];
            if (i == unit_index) {
                height = tile->height * 2 + (tile->depth_half_height & 0x1f);
                low = height - 10;
                if (low < 0) {
                    low = 0;
                }
                high = height + 4;
            } else {
                panel->unit_record_index = 1;
            }
        }
    }
    for (i = 0; i < g_battle_relocated_unit_count; i++) {
        index = g_battle_relocated_unit_coords[i][2] * 256 + g_battle_relocated_unit_coords[i][1] * g_map_max_x
            + g_battle_relocated_unit_coords[i][0];
        panel = &g_battle_target_panels[index];
        panel->unit_record_index = 1;
    }
    for (pass = 0; pass < 2; pass++) {
        limit = 4;
        if (pass != 0) {
            limit = 0x11;
        }
        for (i = 1; i < limit; i++) {
            for (dx = -i; dx <= i; dx++) {
                x = origin_x + dx;
                if (x < 0 || x >= g_map_max_x) {
                    continue;
                }
                if (dx > 0) {
                    dy = i - dx;
                } else {
                    dy = i + dx;
                }
                side = -1;
                found_x = x;
                for (y = origin_y - dy; side < 2; side += 2) {
                    if (y >= 0 && y < g_map_max_y) {
                        found_y = y;
                        for (level = 0; level < 2; level++) {
                            index = level * 256 + g_map_max_x * y + x;
                            tile = &g_battle_map_tile_data[index];
                            depth = tile->depth_half_height;
                            if ((s32)(depth >> 5) >= 3) {
                                continue;
                            }
                            height = depth & 0x1f;
                            if (height >= 3) {
                                continue;
                            }
                            flags = tile->flags_06.value;
                            if (flags & MAP_TILE_FLAG_BLOCKED) {
                                continue;
                            }
                            if (flags & MAP_TILE_FLAG_UNTARGETABLE) {
                                continue;
                            }
                            if (g_battle_target_panels[index].unit_record_index != 0) {
                                continue;
                            }
                            surface = tile->surface.value & 0x3f;
                            if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
                                continue;
                            }
                            if (pass == 0) {
                                total = tile->height * 2 + height;
                                if (total > high || total < low) {
                                    continue;
                                }
                            }
                            destination->x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count][0] = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count][1] = destination->y = found_y;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count][2] = destination->level
                                = level;
                            g_battle_relocated_unit_count++;
                            return 0;
                        }
                    }
                    y += dy * 2;
                }
            }
        }
    }
    return -1;
}

#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
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
s32 battle_unit_find_relocation_tile(s32 unit_index, map_tile_coordinates_t* destination) {
    unit_stats_t* unit;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
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
    map_tile_flags_t flags;
    s32 found_x;
    s32 found_y;
    s32 total;
    target_panel_t* panel;

    unit = &g_battle_unit_stats[unit_index];
    origin_x = unit->x;
    origin_y = unit->position.y;
    for (n = 0x1ff * sizeof(target_panel_t); n >= 0; n -= sizeof(target_panel_t)) {
        ((target_panel_t*)((u8*)g_target_panels + n))->unit_record_index = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            index = map_calculate_location(unit);
            panel = &g_target_panels[index];
            tile = &g_map_tile_data[index];
            if (i == unit_index) {
                height = tile->height * 2 + tile->depth_half_height.half_height;
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
        index = g_battle_relocated_unit_coords[i].elevation * 256 + g_battle_relocated_unit_coords[i].y * g_map_max_x
            + g_battle_relocated_unit_coords[i].x;
        panel = &g_target_panels[index];
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
                            tile = &g_map_tile_data[index];
                            depth = (*(u8*)&tile->depth_half_height);
                            if (tile->depth_half_height.depth >= 3) {
                                continue;
                            }
                            height = depth & MAP_TILE_HALF_HEIGHT_MASK;
                            if (height >= 3) {
                                continue;
                            }
                            flags = tile->flags;
                            if (flags.blocked) {
                                continue;
                            }
                            if (flags.untargetable) {
                                continue;
                            }
                            if (g_target_panels[index].unit_record_index != 0) {
                                continue;
                            }
                            surface = tile->surface.type;
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
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].x = found_x;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].y = destination->y = found_y;
                            g_battle_relocated_unit_coords[g_battle_relocated_unit_count].elevation = destination->layer
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

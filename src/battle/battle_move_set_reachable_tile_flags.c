#include "fft/battle.h"
#include "psx/types.h"

enum {
    MAP_TILE_COUNT = 512,
};

/* Mark every tile the acting unit can end its move on and count them.
 *
 * Teleporting units may land on any valid-destination tile and riders on
 * any mountable panel. Otherwise the tile needs remaining range, a low
 * enough slope, a passable surface and depth the unit tolerates, and must be
 * crossable and either empty or a valid destination the unit can ride onto. */
void battle_move_set_reachable_tile_flags(void) {
    battle_move_pathfind_scratch_t* config;
    battle_move_spread_state_t* spread;
    u8* flags;
    s32 self;
    s32 i;
    map_tile_t* tile;
    battle_target_panel_t* panel;
    u32 depth;
    s32 half_height;
    s32 surface;
    s32 tile_flags;
    s32 depth_bits;
    s32 frontier;

    config = g_battle_move_config_ptr;
    spread = g_battle_move_scratch_pad_ptr;
    flags = g_battle_move_frontier_flags_ptr;
    self = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    battle_move_clear_reachable_flags_under_dead_or_jumping_units();
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks &= ~MAP_TILE_FLAG_MOVE_DESTINATION;
        panel = &g_battle_target_panels[i];
        if (i == self) {
            continue;
        }
        if (!((config->move_type == BATTLE_MOVEMENT_CLASS_TELEPORT) && (flags[i] & FRONTIER_FLAG_VALID_DESTINATION))
            && !((panel->ride_remaining_range != 0) && (config->can_ride != 0))) {
            depth = tile->depth_half_height;
            half_height = depth & MAP_TILE_HALF_HEIGHT_MASK;
            if ((half_height >= 3) && (flags[i] & FRONTIER_FLAG_LOW_CORNER)) {
                continue;
            }
            if (panel->remaining_range == 0) {
                continue;
            }
            if (half_height >= 4) {
                continue;
            }
            tile_flags = tile->flags_06.value;
            if (tile_flags & MAP_TILE_FLAG_CANNOT_STOP) {
                continue;
            }
            surface = tile->surface.value & MAP_SURFACE_MASK;
            if (surface == MAP_SURFACE_OBSTACLE) {
                continue;
            }
            depth_bits = tile->depth_half_height & MAP_TILE_DEPTH_MASK;
            if ((depth_bits != 0) && (config->cannot_stay_on_water != 0)) {
                continue;
            }
            if ((depth_bits == 0x60) && (config->will_drown != 0)) {
                continue;
            }
            if (((s32)(depth >> MAP_TILE_DEPTH_SHIFT) >= 4) && (config->will_sink != 0)) {
                continue;
            }
            if ((surface == MAP_SURFACE_LAVA) && (config->can_pass_lava == 0)) {
                continue;
            }
            if (tile_flags & MAP_TILE_FLAG_UNTARGETABLE) {
                continue;
            }
            frontier = flags[i];
            if (!(frontier & FRONTIER_FLAG_REACHABLE)) {
                continue;
            }
            if (frontier & FRONTIER_FLAG_UNIT_ON_TILE) {
                if (!(frontier & FRONTIER_FLAG_VALID_DESTINATION) || (config->can_ride == 0)) {
                    continue;
                }
            }
        }
        spread->reachable_count++;
        tile->ceiling_depth_and_marks |= MAP_TILE_FLAG_MOVE_DESTINATION;
    }
}

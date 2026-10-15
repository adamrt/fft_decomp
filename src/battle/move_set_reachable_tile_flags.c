#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Mark every tile the acting unit can end its move on and count them.
 *
 * Teleporting units may land on any valid-destination tile and riders on
 * any mountable panel. Otherwise the tile needs remaining range, a low
 * enough slope, a passable surface and depth the unit tolerates, and must be
 * crossable and either empty or a valid destination the unit can ride onto. */
void move_set_reachable_tile_flags(void) {
    move_pathfind_scratch_t* config;
    move_spread_state_t* spread;
    u8* flags;
    s32 self;
    s32 i;
    map_tile_t* tile;
    target_panel_t* panel;
    s32 half_height;
    s32 surface;
    map_tile_flags_t tile_flags;
    map_tile_depth_t water_depth;
    s32 frontier;

    config = g_move_config_ptr;
    spread = g_move_scratch_pad_ptr;
    flags = g_move_frontier_flags_ptr;
    self = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    move_clear_enterable_flags_under_dead_or_jumping_units();
    for (i = 0; i < MAP_TILE_COUNT; i++) {
        tile = &g_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        panel = &g_target_panels[i];
        if (i == self) {
            continue;
        }
        if (!((config->move_type == MOVE_CLASS_TELEPORT) && (flags[i] & FRONTIER_FLAG_MOUNTABLE_UNIT))
            && !((panel->ride_remaining_range != 0) && (config->can_ride != 0))) {
            half_height = tile->depth_half_height.half_height;
            if ((half_height >= 3) && (flags[i] & FRONTIER_FLAG_LOW_CORNER)) {
                continue;
            }
            if (panel->remaining_range == 0) {
                continue;
            }
            if (half_height >= 4) {
                continue;
            }
            tile_flags = tile->flags;
            if (tile_flags.cannot_stop) {
                continue;
            }
            surface = tile->surface.type;
            if (surface == MAP_SURFACE_OBSTACLE) {
                continue;
            }
            water_depth = tile->depth_half_height;
            if ((water_depth.depth != 0) && (config->cannot_stay_on_water != 0)) {
                continue;
            }
            if ((water_depth.depth == 3) && (config->will_drown != 0)) {
                continue;
            }
            if ((tile->depth_half_height.depth >= 4) && (config->will_sink != 0)) {
                continue;
            }
            if ((surface == MAP_SURFACE_LAVA) && (config->can_pass_lava == 0)) {
                continue;
            }
            if (tile_flags.untargetable) {
                continue;
            }
            frontier = flags[i];
            if (!(frontier & FRONTIER_FLAG_ENTERABLE)) {
                continue;
            }
            if (frontier & FRONTIER_FLAG_UNIT_ON_TILE) {
                if (!(frontier & FRONTIER_FLAG_MOUNTABLE_UNIT) || (config->can_ride == 0)) {
                    continue;
                }
            }
        }
        spread->reachable_count++;
        tile->ceiling_depth_and_marks.move_destination = 1;
    }
}

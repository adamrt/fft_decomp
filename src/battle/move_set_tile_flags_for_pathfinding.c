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

/* Rebuild the frontier flags of all terrain tiles from slope, half height, depth
 * and the tile flags, then mark the tiles under the 16 unit records. A unit of
 * another team makes its tile not enterable (a flier may still cross on top of
 * it). One of the mover's team can be mounted unless its record says
 * otherwise, and stood on when it is a stepping stone and the mover is not
 * one. Altima's second form blocks its tile.
 *
 * A mode with bit 0 set also clears the panel ranges and the reachable count.
 * START_SPREAD queues the mover's own tile with Move + 1 range (and the unit
 * record on that tile when it is mountable); TERRAIN_ONLY leaves the units
 * out. */
void move_set_tile_flags_for_pathfinding(s32 mode) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* records = g_move_records_ptr;
    move_record_t* record;
    target_panel_t* panel;
    target_panel_t* aux;
    map_tile_t* tile;
    s32 i;
    s32 index;
    s32 height;
    u8* frontier_cell;
    u8* unit_cell;
    u8 slope;
    map_tile_flags_t tile_flags;
    u8 flags;
    u8 depth;
    u8 value;

    for (i = 0, frontier_cell = frontier; i < MOVE_PANEL_COUNT; frontier_cell++, i++) {
        panel = &g_target_panels[i];
        panel->mark = 0;
        panel->unit_record_index = 0;
        if (mode & 1) {
            panel->remaining_range = 0;
            panel->ride_remaining_range = 0;
            panel->max_height_delta = 0xff;
        }
        if (i < MAP_TILE_COUNT) {
            tile = &g_map_tile_data[i];
            frontier[i] = 0;
            slope = tile->slope_type & 5;
            if (slope == 1 || slope == 4) {
                *frontier_cell |= FRONTIER_FLAG_LOW_CORNER;
                if (tile->depth_half_height.half_height >= 3) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            } else {
                if (tile->depth_half_height.half_height > config->jump_or_1f) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            }
            if ((tile->flags.blocked | tile->flags.untargetable) == 0) {
                depth = tile->depth_half_height.depth;
                if ((depth == 0 || !config->cannot_enter_water) && (depth < 4 || !config->will_sink)) {
                    *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
                }
            }
            tile_flags = tile->flags;
            if (tile_flags.blocked) {
                *frontier_cell |= FRONTIER_FLAG_BLOCKED;
            } else if (!config->move_type && (tile_flags.untargetable)
                && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
                *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
            }
        }
    }
    if (mode != MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY) {
        for (i = 0; i < MOVE_RECORD_COUNT; i++) {
            record = &records[i];
            flags = record->unit_id_flags;
            if (flags != MOVE_RECORD_NONE) {
                index = (record->higher_elevation << 8) + record->y * config->map_max_x + record->x;
                if (g_battle_unit_stats[flags & MOVE_RECORD_UNIT_ID_MASK].character_identity
                    == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
                    frontier[index] = (frontier[index] | FRONTIER_FLAG_BLOCKED) & ~FRONTIER_FLAG_ENTERABLE;
                }
                g_target_panels[index].unit_record_index = i;
                unit_cell = &frontier[index];
                *unit_cell |= FRONTIER_FLAG_UNIT_ON_TILE;
                if (!(flags & MOVE_RECORD_FLAG_OTHER_TEAM)) {
                    if (!config->stepping_stone) {
                        height = record->stepping_stone << 3;
                        *unit_cell = (*unit_cell & ~FRONTIER_FLAG_STAND_ON_UNIT) | height;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                    if (!(flags & MOVE_RECORD_FLAG_NOT_MOUNTABLE)) {
                        frontier[index] |= FRONTIER_FLAG_MOUNTABLE_UNIT;
                    } else {
                        frontier[index] &= ~FRONTIER_FLAG_MOUNTABLE_UNIT;
                    }
                } else {
                    *unit_cell &= ~(FRONTIER_FLAG_ENTERABLE | FRONTIER_FLAG_MOUNTABLE_UNIT);
                    if (!config->move_type) {
                        *unit_cell |= FRONTIER_FLAG_STAND_ON_UNIT;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                }
            }
        }
    }
    if (mode & 1) {
        state->reachable_count = 0;
        index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    }
    if (mode == MOVE_TILE_FLAGS_MODE_START_SPREAD) {
        panel = &g_target_panels[index];
        if (frontier[index] & FRONTIER_FLAG_MOUNTABLE_UNIT) {
            state->outer_count = 0x24;
            aux = g_target_panels + MAP_TILE_COUNT + panel->unit_record_index;
            aux->remaining_range = config->move + 1;
            aux->max_height_delta = 0;
        } else {
            state->outer_count = 8;
        }
        state->inner_count = 4;
        panel->remaining_range = config->move + 1;
        panel->max_height_delta = 0;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
    }
}

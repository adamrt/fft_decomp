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

/* padding */

/* Encode the traced path as move_step_t bytes in g_move_path,
 * count holding the number of encoded steps. Starting from the unit's tile, step n goes to
 * the tile or unit record the back-trace marked with path_length - n; the byte
 * records the direction, the distance, the level, whether the step ends on a
 * unit and whether either end is a climb. */
void move_encode_path_steps(void) {
    move_pathfind_scratch_t* config = g_move_config_ptr;
    move_spread_state_t* state = g_move_scratch_pad_ptr;
    move_record_t* records = g_move_records_ptr;
    u8* frontier = g_move_frontier_flags_ptr;
    move_record_t* record;
    map_tile_t* tile;
    s32 step;
    s32 path_index;
    u8 level;
    s32 i;
    s32 target;
    s32 rem;
    u8 hit;
    u8 dir;
    s32 flag;
    u8 source_bit;
    u8 destination_bit;

    state->source_unit_record_index = 0;
    for (step = 0; step < state->path_length - 1; step++) {
        state->tile_index = state->tile_level * MAP_TILES_PER_LAYER + state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        path_index = step + 1;
        target = state->path_length - path_index;
        for (i = 0; i < MOVE_PANEL_COUNT; i++) {
            if (i >= MAP_TILE_COUNT && g_target_panels[i].ride_remaining_range == (target & 0xff)) {
                hit = 1;
                record = &records[i - MAP_TILE_COUNT];
            } else if (g_target_panels[i].mark == (target & 0xff)) {
                hit = 0;
                if (i < MAP_TILE_COUNT) {
                    state->destination_unit_record_index = 0;
                    state->work_level = (u8)(i / MAP_TILES_PER_LAYER);
                    rem = i - (i / MAP_TILES_PER_LAYER) * MAP_TILES_PER_LAYER;
                    state->work_y = (u8)(rem / config->map_max_x);
                    state->work_x = (u8)(rem % config->map_max_x);
                } else {
                    state->destination_unit_record_index = 1;
                    record = &records[i - MAP_TILE_COUNT];
                }
            } else {
                continue;
            }
            if (state->destination_unit_record_index || hit) {
                state->work_x = record->x;
                state->work_y = record->y;
                state->work_level = record->higher_elevation;
            }
            if (state->work_x == state->tile_x) {
                dir = 0xc0;
                if (state->work_y > state->tile_y) {
                    state->source_side_shift = 6;
                    state->destination_side_shift = 4;
                    state->step_distance = state->work_y - state->tile_y;
                } else {
                    dir = 0x80;
                    state->source_side_shift = 4;
                    state->destination_side_shift = 6;
                    state->step_distance = state->tile_y - state->work_y;
                }
            } else {
                dir = 0;
                if (state->tile_x < state->work_x) {
                    state->source_side_shift = 0;
                    state->destination_side_shift = 2;
                    state->step_distance = state->work_x - state->tile_x;
                } else {
                    dir = 0x40;
                    state->source_side_shift = 2;
                    state->destination_side_shift = 0;
                    state->step_distance = state->tile_x - state->work_x;
                }
            }
            state->destination_climb = 0;
            if (!state->destination_unit_record_index && !hit) {
                tile = &g_map_tile_data[i];
                state->destination_entry_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->source_side_shift) & 3);
                state->destination_opposite_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->destination_side_shift) & 3);
                state->destination_side_height_delta
                    = state->destination_entry_height - state->destination_opposite_height;
                if (config->jump_or_1f < tile->depth_half_height.half_height) {
                    state->destination_climb = 1;
                }
                if (tile->depth_half_height.half_height >= 3 && (frontier[i] & FRONTIER_FLAG_LOW_CORNER)) {
                    state->destination_climb = 1;
                }
                if (state->destination_side_height_delta >= 0) {
                    state->destination_climb = 0;
                }
            }
            state->source_climb = 0;
            if (!state->source_unit_record_index) {
                if (config->jump_or_1f < state->current_tile->depth_half_height.half_height) {
                    state->source_climb = 1;
                }
                if (state->current_tile->depth_half_height.half_height >= 3
                    && (frontier[state->tile_index] & FRONTIER_FLAG_LOW_CORNER)) {
                    state->source_climb = 1;
                }
            }
            level = state->work_level * 32;
            state->source_unit_record_index = state->destination_unit_record_index;
            flag = state->destination_unit_record_index << 4;
            if (step == state->path_length - 2 && g_move_path_height_offsets.source_present) {
                flag = 0x10;
            }
            source_bit = state->source_climb << 2;
            destination_bit = state->destination_climb << 3;
            /* Pack the complete path byte; individual field stores add truncation and change scheduling. */
            *(u8*)&g_move_path.data.steps[step]
                = (state->step_distance - 1) | (source_bit | (destination_bit | (flag | (dir | level))));
            state->tile_x = state->work_x;
            state->tile_y = state->work_y;
            state->tile_level = state->work_level;
            break;
        }
    }
    g_move_path.count = state->path_length - 1;
    if (config->unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        g_move_effective_flags = 0;
    } else {
        g_move_effective_flags = move_get_effective_flags(&g_battle_unit_stats[config->unit_id]);
    }
}

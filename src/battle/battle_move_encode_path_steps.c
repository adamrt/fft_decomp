#include "fft/battle.h"

void battle_move_encode_path_steps(void) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    battle_move_record_t* record;
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
    for (step = 0; step < state->_pad5c[2] - 1; step++) {
        state->tile_index = state->tile_level * 256 + (s16)state->tile_y * config->map_max_x + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_battle_target_panels[state->tile_index];
        path_index = step + 1;
        target = state->_pad5c[2] - path_index;
        for (i = 0; i < 0x210; i++) {
            if (i >= 0x200 && g_battle_target_panels[i].ride_remaining_range == (target & 0xff)) {
                hit = 1;
                record = &records[i - 0x200];
            } else if (g_battle_target_panels[i].mark == (target & 0xff)) {
                hit = 0;
                if (i < 0x200) {
                    state->destination_unit_record_index = 0;
                    state->work_level = (u8)(i / 256);
                    rem = i - (i / 256) * 256;
                    state->work_y = (u8)(rem / config->map_max_x);
                    state->work_x = (u8)(rem % config->map_max_x);
                } else {
                    state->destination_unit_record_index = 1;
                    record = &records[i - 0x200];
                }
            } else {
                continue;
            }
            if (state->destination_unit_record_index || hit) {
                state->work_x = record->x;
                state->work_y = record->y;
                state->work_level = record->higher_elevation;
            }
            if (state->work_x == (s16)state->tile_x) {
                dir = 0xc0;
                if (state->work_y > (s16)state->tile_y) {
                    state->source_side_shift = 6;
                    state->destination_side_shift = 4;
                    state->_pad5c[5] = state->work_y - state->tile_y;
                } else {
                    dir = 0x80;
                    state->source_side_shift = 4;
                    state->destination_side_shift = 6;
                    state->_pad5c[5] = state->tile_y - state->work_y;
                }
            } else {
                dir = 0;
                if ((s16)state->tile_x < state->work_x) {
                    state->source_side_shift = 0;
                    state->destination_side_shift = 2;
                    state->_pad5c[5] = state->work_x - state->tile_x;
                } else {
                    dir = 0x40;
                    state->source_side_shift = 2;
                    state->destination_side_shift = 0;
                    state->_pad5c[5] = state->tile_x - state->work_x;
                }
            }
            state->_pad5c[4] = 0;
            if (!state->destination_unit_record_index && !hit) {
                tile = &g_battle_map_tile_data[i];
                state->destination_entry_height
                    = ((s8)tile->depth_half_height & 0x1f) * ((tile->slope_type >> state->source_side_shift) & 3);
                state->destination_opposite_height
                    = ((s8)tile->depth_half_height & 0x1f) * ((tile->slope_type >> state->destination_side_shift) & 3);
                state->destination_side_height_delta
                    = state->destination_entry_height - state->destination_opposite_height;
                if (config->jump_or_1f < ((s8)tile->depth_half_height & 0x1f)) {
                    state->_pad5c[4] = 1;
                }
                if (((s8)tile->depth_half_height & 0x1f) >= 3 && (frontier[i] & 2)) {
                    state->_pad5c[4] = 1;
                }
                if (state->destination_side_height_delta >= 0) {
                    state->_pad5c[4] = 0;
                }
            }
            state->_pad5c[3] = 0;
            if (!state->source_unit_record_index) {
                if (config->jump_or_1f < ((s8)state->current_tile->depth_half_height & 0x1f)) {
                    state->_pad5c[3] = 1;
                }
                if (((s8)state->current_tile->depth_half_height & 0x1f) >= 3 && (frontier[state->tile_index] & 2)) {
                    state->_pad5c[3] = 1;
                }
            }
            level = state->work_level * 32;
            state->source_unit_record_index = state->destination_unit_record_index;
            flag = state->destination_unit_record_index << 4;
            if (step == state->_pad5c[2] - 2 && (g_battle_move_path_height_offsets & 0x10)) {
                flag = 0x10;
            }
            source_bit = state->_pad5c[3] << 2;
            destination_bit = state->_pad5c[4] << 3;
            g_battle_move_path[path_index]
                = (state->_pad5c[5] - 1) | (source_bit | (destination_bit | (flag | (dir | level))));
            state->tile_x = state->work_x;
            state->tile_y = state->work_y;
            state->tile_level = state->work_level;
            break;
        }
    }
    g_battle_move_path[0] = state->_pad5c[2] - 1;
    if (config->unit_id >= 0x15) {
        g_battle_move_effective_flags = 0;
    } else {
        g_battle_move_effective_flags = battle_move_get_effective_flags(&g_battle_unit_stats[config->unit_id]);
    }
}

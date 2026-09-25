#include "fft/battle.h"

void battle_move_set_tile_flags_for_pathfinding(s32 mode) {
    battle_move_pathfind_scratch_t* config = g_battle_move_config_ptr;
    battle_move_spread_state_t* state = g_battle_move_scratch_pad_ptr;
    u8* frontier = g_battle_move_frontier_flags_ptr;
    battle_move_record_t* records = g_battle_move_records_ptr;
    battle_move_record_t* record;
    battle_target_panel_t* panel;
    battle_target_panel_t* aux;
    map_tile_t* tile;
    s32 i;
    s32 index;
    s32 height;
    u8* frontier_cell;
    u8* unit_cell;
    u8 slope;
    u8 tile_flags;
    u8 flags;
    u8 depth;
    u8 value;

    for (i = 0, frontier_cell = frontier; i < 0x210; frontier_cell++, i++) {
        panel = &g_battle_target_panels[i];
        panel->mark = 0;
        panel->unit_record_index = 0;
        if (mode & 1) {
            panel->remaining_range = 0;
            panel->ride_remaining_range = 0;
            panel->max_height_delta = 0xff;
        }
        if (i < 0x200) {
            tile = &g_battle_map_tile_data[i];
            frontier[i] = 0;
            slope = tile->slope_type & 5;
            if (slope == 1 || slope == 4) {
                *frontier_cell |= 2;
                if (((s8)tile->depth_half_height & 0x1f) >= 3) {
                    *frontier_cell |= 0x40;
                }
            } else {
                if (((s8)tile->depth_half_height & 0x1f) > config->jump_or_1f) {
                    *frontier_cell |= 0x40;
                }
            }
            if ((tile->flags_06.bits.blocked | tile->flags_06.bits.untargetable) == 0) {
                depth = tile->depth_half_height >> 5;
                if ((depth == 0 || !config->cannot_enter_water) && (depth < 4 || !config->will_sink)) {
                    *frontier_cell |= 0x10;
                }
            }
            tile_flags = tile->flags_06.value;
            if (tile_flags & 1) {
                *frontier_cell |= 0x80;
            } else if (!config->move_type && (tile_flags & 2) && (tile->surface.value & 0x3f) != 0x3f) {
                *frontier_cell |= 0x10;
            }
        }
    }
    if (mode != 3) {
        for (i = 0; i < 16; i++) {
            record = &records[i];
            flags = record->unit_id_flags;
            if (flags != 0xff) {
                index = (record->higher_elevation << 8) + record->y * config->map_max_x + record->x;
                if (g_battle_unit_stats[flags & 0x1f].character_identity == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
                    frontier[index] = (frontier[index] | 0x80) & 0xef;
                }
                g_battle_target_panels[index].unit_record_index = i;
                unit_cell = &frontier[index];
                *unit_cell |= 4;
                if (!(flags & 0x40)) {
                    if (!config->stepping_stone) {
                        height = record->stepping_stone << 3;
                        *unit_cell = (*unit_cell & 0xf7) | height;
                    } else {
                        *unit_cell &= 0xf7;
                    }
                    if (!(flags & 0x20)) {
                        frontier[index] |= 0x20;
                    } else {
                        frontier[index] &= 0xdf;
                    }
                } else {
                    *unit_cell &= 0xcf;
                    if (!config->move_type) {
                        *unit_cell |= 8;
                    } else {
                        *unit_cell &= 0xf7;
                    }
                }
            }
        }
    }
    if (mode & 1) {
        state->reachable_count = 0;
        index = (config->high_elevation << 8) + config->y * config->map_max_x + config->x;
    }
    if (mode == 1) {
        panel = &g_battle_target_panels[index];
        if (frontier[index] & 0x20) {
            state->outer_count = 0x24;
            aux = g_battle_target_panels + 0x200 + panel->unit_record_index;
            aux->remaining_range = config->move + 1;
            aux->max_height_delta = 0;
        } else {
            state->outer_count = 8;
        }
        state->inner_count = 4;
        panel->remaining_range = config->move + 1;
        panel->max_height_delta = 0;
        frontier[index] |= 1;
        state->frontier_max_remaining_range = config->move + 1;
    }
}

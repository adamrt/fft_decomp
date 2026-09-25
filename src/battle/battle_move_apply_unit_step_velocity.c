#include "fft/battle.h"
#include "psx/types.h"

void battle_move_apply_unit_step_velocity(battle_unit_misc_data_t* unit) {
    s16 tile[2];
    u8 layer;
    s16 ground_z;
    s16 move_ground_z;
    map_tile_t* map_tile;
    battle_unit_misc_data_t* rider;
    s32 depth;

    tile[0] = (unit->real.vx + unit->velocity.vx) / 0x1C000;
    tile[1] = (unit->real.vz + unit->velocity.vz) / 0x1C000;
    layer = unit->map_z;
    if (tile[0] == unit->movement.bytes.destination_x && tile[1] == unit->movement.bytes.destination_y
        && unit->map_z != unit->movement.bytes.destination_z) {
        layer = unit->map_z = unit->movement.bytes.destination_z;
    }
    switch (unit->centre_tile_offset) {
    case 2:
    case 4:
    case 6:
    case 8:
        if (tile[0] == unit->movement.bytes.destination_x && tile[1] == unit->movement.bytes.destination_y
            && unit->map_z != unit->movement.bytes.destination_z) {
            layer = unit->map_z = unit->movement.bytes.destination_z;
            battle_move_set_unit_step_delta_edge_to_center(unit, &unit->movement_value,
                battle_map_get_tile_data_pointer(tile[0], tile[1], layer), unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        }
        break;
    case 10:
    case 12:
    case 14:
    case 16:
        if (tile[0] == unit->movement.bytes.destination_x && tile[1] == unit->movement.bytes.destination_y
            && unit->map_z != unit->movement.bytes.destination_z) {
            layer = unit->map_z = unit->movement.bytes.destination_z;
            battle_move_set_unit_step_delta_edge_to_center(unit, &unit->movement_value,
                battle_map_get_tile_data_pointer(tile[0], tile[1], layer), unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->step_speed);
        }
        break;
    }
    unit->real.vx += unit->velocity.vx;
    unit->real.vz += unit->velocity.vz;
    unit->screen.vx = unit->real.vx / 4096;
    unit->screen.vz = unit->real.vz / 4096;
    ground_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    move_ground_z = battle_gfx_calculate_screen_z_from_misc_move_data(unit);
    switch (unit->centre_tile_offset) {
    case 1:
    case 3:
    case 5:
    case 7:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / 4096) <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_unit_step_delta_center_to_edge(
                unit, &unit->movement_value, map_tile, unit->current_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        } else {
            if ((-unit->real.vy / 4096) < -ground_z + 6) {
                unit->real.vy = ground_z << 12;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_battle_move_jump_gravity;
            }
            if (-unit->real.vy / 4096 >= -ground_z + 0x2A) {
                unit->centre_tile_offset = 0x1A;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(0x1F, (s16)unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 9:
    case 11:
    case 13:
    case 15:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / 4096) <= -ground_z) {
            unit->real.vy = ground_z << 12;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_battle_move_jump_gravity;
            }
            battle_move_set_unit_step_delta_center_to_edge(
                unit, &unit->movement_value, map_tile, unit->current_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->step_speed);
        } else {
            if ((-unit->real.vy / 4096) < -ground_z + 6) {
                unit->real.vy = ground_z << 12;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_battle_move_jump_gravity;
            }
            if (-unit->real.vy / 4096 >= -ground_z + 0x2A) {
                unit->centre_tile_offset = 0x1A;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(0x1F, (s16)unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 2:
    case 4:
    case 6:
    case 8:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_unit_step_delta_edge_to_center(
                unit, &unit->movement_value, map_tile, unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        }
        if (-ground_z < (-unit->real.vy / 4096)) {
            if ((-unit->real.vy / 4096) < -ground_z + 6) {
                unit->real.vy = ground_z << 12;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_battle_move_jump_gravity;
            }
            if (-unit->real.vy / 4096 >= -move_ground_z + 0x2A) {
                unit->centre_tile_offset = 0x1B;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(0x1F, (s16)unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 10:
    case 12:
    case 14:
    case 16:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_battle_move_jump_gravity;
            }
            battle_move_set_unit_step_delta_edge_to_center(
                unit, &unit->movement_value, map_tile, unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->step_speed);
        }
        if ((-unit->real.vy / 4096) > -ground_z) {
            if ((-unit->real.vy / 4096) < -ground_z + 6) {
                unit->real.vy = ground_z << 12;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_battle_move_jump_gravity;
            }
            if (-unit->real.vy / 4096 >= -move_ground_z + 0x2A) {
                unit->centre_tile_offset = 0x1B;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(0x1F, (s16)unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x17:
    case 0x1A:
    case 0x1B:
    case 0x1E:
    case 0x1F:
    case 0x2D:
    case 0x31:
    case 0x35:
    case 0x39:
        if (unit->velocity.vy <= 0x7FFF) {
            unit->velocity.vy += g_battle_move_jump_gravity;
        }
        unit->real.vy += unit->velocity.vy;
        if (unit->velocity.vy > 0) {
            if (-unit->real.vy / 4096 < -ground_z) {
                unit->real.vy = ground_z << 12;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 0x2B:
    case 0x2F:
    case 0x33:
    case 0x37:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_float_step_delta_center_to_edge(
                unit, &unit->movement_value, map_tile, unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        } else {
            /* Shared with the next case: a duplicated branch does not merge. */
            goto accelerate;
        }
        unit->shadow_dirty |= 1;
        break;
    case 0x2C:
    case 0x30:
    case 0x34:
    case 0x38:
        unit->real.vy += unit->velocity.vy;
        map_tile = battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_float_step_delta_edge_to_center(
                unit, &unit->movement_value, map_tile, unit->destination_edge_height);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        } else {
        accelerate:
            /* Keeps depth live here, so it takes a0 and reorg leaves `move a0,s0` out of both landing tests. */
            __asm__ volatile("" : : "r"(depth));
            if (unit->velocity.vy < 0x2000) {
                unit->velocity.vy += g_battle_move_jump_gravity;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case 0x22:
    case 0x24:
    case 0x26:
    case 0x28:
        unit->real.vy += unit->velocity.vy;
        battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_velocity_for_contiguous_clamped_steps(
                unit, &unit->movement_path_count, &unit->movement_value);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        }
        unit->shadow_dirty |= 1;
        break;
    case 0x23:
    case 0x25:
    case 0x27:
    case 0x29:
        unit->real.vy += unit->velocity.vy;
        battle_map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / 4096 <= -ground_z) {
            unit->real.vy = ground_z << 12;
            battle_move_set_velocity_for_contiguous_steps_with_final_tile_height(
                unit, &unit->movement_path_count, &unit->movement_value);
            battle_move_interpolate_partial((s32*)unit, unit->walk_speed.word);
        }
        unit->shadow_dirty |= 1;
        break;
    }
    battle_unit_set_screen_coords_from_real_coords(unit);
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        rider = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (rider != 0) {
            rider->screen = unit->screen;
            rider->screen.vy -= 10;
            rider->real = unit->real;
            rider->real.vy -= 0xA000;
        }
    }
}

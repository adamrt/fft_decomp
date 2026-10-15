#include "fft/battle_unit.h"

/* Turn a unit to face a target unit, or its action's target.
 *
 * Without an explicit target, targeting type 5 aims at the center of the
 * action's target tile (28 screen units per tile) and other actions at the
 * target unit. The facing snaps to a quadrant, is copied to the mount/rider
 * partner, and is written back to the unit's battle position; units with
 * Sleep, Petrify, Stop, Dead, Don't Act, or reaction_ability_id 0x1b8 (Reflect) keep their
 * facing. */
void battle_unit_face_towards_action_target(battle_unit_t* unit, battle_unit_t* target) {
    VECTOR delta;
    battle_unit_t* other;

    if (unit == 0) {
        return;
    }
    delta.vz = 0;
    delta.vx = 0;
    if (target != 0) {
        delta.vx = target->world_coords.vx - unit->world_coords.vx;
        delta.vz = target->world_coords.vz - unit->world_coords.vz;
    } else if (unit->stats->action_command.targeting_type == BATTLE_TARGETING_TYPE_TILE) {
        delta.vx = unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
            - unit->world_coords.vx;
        delta.vz = unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
            - unit->world_coords.vz;
    } else {
        other = battle_unit_get_by_battle_id(unit->stats->action_command.target_id);
        if (other != 0) {
            delta.vx = other->world_coords.vx - unit->world_coords.vx;
            delta.vz = other->world_coords.vz - unit->world_coords.vz;
        }
    }
    if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PREVENT_FACING_MASK) {
        return;
    }
    if (unit->strike.reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
        return;
    }
    if (delta.vx == 0 && delta.vz == 0) {
        return;
    }
    battle_unit_save_facing_before_attack(unit);
    unit->facing = (ratan2(-delta.vz, delta.vx) - 0x200) & 0xc00;
    battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id)->facing = unit->facing;
        battle_unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
    }
}

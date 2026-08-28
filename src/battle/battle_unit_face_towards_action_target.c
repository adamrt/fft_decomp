#include "fft/battle.h"
#include "fft/data.h"

/* Turn a unit to face a target unit, or its action's target.
 *
 * Without an explicit target, targeting type 5 aims at the centre of the
 * action's target tile (28 screen units per tile) and other actions at the
 * target unit. The facing snaps to a quadrant, is copied to the mount/rider
 * partner, and is written back to the unit's battle position; units with
 * Sleep, Petrify, Stop, Dead, Don't Act, or reaction_ability_id 0x1b8 (Reflect) keep their
 * facing. */
void battle_unit_face_towards_action_target(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* target) {
    VECTOR delta;
    battle_unit_misc_data_t* other;

    if (unit == 0) {
        return;
    }
    delta.vz = 0;
    delta.vx = 0;
    if (target != 0) {
        delta.vx = target->screen.vx - unit->screen.vx;
        delta.vz = target->screen.vz - unit->screen.vz;
    } else if (unit->battle_data->action_target_kind == BATTLE_ACTION_TARGET_TILE) {
        delta.vx = unit->battle_data->action_target_x * 28 + 14 - unit->screen.vx;
        delta.vz = unit->battle_data->action_target_y * 28 + 14 - unit->screen.vz;
    } else {
        other = battle_unit_get_misc_data_by_battle_id(unit->battle_data->action_target_id);
        if (other != 0) {
            delta.vx = other->screen.vx - unit->screen.vx;
            delta.vz = other->screen.vz - unit->screen.vz;
        }
    }
    if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_PREVENT_FACING_MASK) {
        return;
    }
    if (unit->reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
        return;
    }
    if (delta.vx == 0 && delta.vz == 0) {
        return;
    }
    battle_unit_save_facing_before_attack(unit);
    unit->facing = (ratan2(-delta.vz, delta.vx) - 0x200) & 0xc00;
    battle_unit_set_tile_position(
        unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, (s16)unit->facing / 1024);
    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
        battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id)->facing = unit->facing;
        battle_unit_set_tile_position(
            unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, (s16)unit->facing / 1024);
    }
}

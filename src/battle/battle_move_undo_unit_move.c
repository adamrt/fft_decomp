#include "fft/battle.h"

void battle_move_undo_unit_move(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* other;

    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        other->mount_state = BATTLE_MISC_MOUNT_STATE_NONE;
        battle_unit_update_display_by_misc_id(other->unit_id);
    }
    battle_move_return_to_previous_map_square(unit);
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (other != 0) {
            battle_move_return_to_previous_map_square(other);
            battle_unit_update_display_by_misc_id(other->unit_id);
        }
    } else if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        unit->status_flags_1_4 |= BATTLE_MISC_STATUS_MOUNTED;
        other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (other != 0) {
            other->mount_state = BATTLE_MISC_MOUNT_STATE_MOUNT;
            other->mount_partner_misc_id = unit->unit_id;
            battle_unit_update_display_by_misc_id(other->unit_id);
        }
    } else {
        unit->status_flags_1_4 &= ~BATTLE_MISC_STATUS_MOUNTED;
    }
    battle_unit_update_display_by_misc_id(unit->unit_id);
}

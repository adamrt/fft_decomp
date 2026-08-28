#include "fft/battle.h"
#include "psx/types.h"

/*
 * Mount the unit onto its pending mount.  Bit 0x80 of mount_byte selects an
 * explicit battle id in the low seven bits; otherwise the mount is whatever
 * chocobo already stands on the unit's tile, and failing that the unit is
 * itself the mount for a rider recorded at mount_partner_misc_id.
 */
void battle_unit_mount_onto_pending_mount(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* other;

    battle_unit_store_coordinate_mount_animation_data(unit);
    if (unit->mount_byte & 0x80) {
        other = battle_unit_get_misc_data_by_battle_id(unit->mount_byte & 0x7f);
        if (other != 0) {
            battle_unit_mount_rider_onto_unit(unit, other);
            battle_sound_play_movement_sfx(unit, 0x29);
        }
        return;
    }

    other = battle_unit_find_other_chocobo_at_map_coords(unit->map_x, unit->map_y, unit->map_z);
    if (other != 0) {
        if (other != unit) {
            battle_unit_mount_rider_onto_unit(unit, other);
        }
        return;
    }

    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (other != 0) {
            battle_unit_mount_rider_onto_unit(other, unit);
        }
    }
}

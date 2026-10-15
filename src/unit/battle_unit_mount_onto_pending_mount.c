#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"

/*
 * Mount the unit onto its pending mount.  Bit 0x80 of pending_mount selects an
 * explicit battle id in the low seven bits; otherwise the mount is whatever
 * chocobo already stands on the unit's tile, and failing that the unit is
 * itself the mount for a rider recorded at mount_partner_runtime_id.
 */
void battle_unit_mount_onto_pending_mount(battle_unit_t* unit) {
    battle_unit_t* other;

    battle_unit_store_coordinate_mount_animation_data(unit);
    if (unit->path.pending_mount & BATTLE_PENDING_MOUNT_SET) {
        other = battle_unit_get_by_battle_id(unit->path.pending_mount & BATTLE_PENDING_MOUNT_ID_MASK);
        if (other != 0) {
            battle_unit_mount_rider_onto_unit(unit, other);
            sound_play_movement_sfx(unit, MAIN_SFX_LAND_DEFAULT);
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

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            battle_unit_mount_rider_onto_unit(other, unit);
        }
    }
}

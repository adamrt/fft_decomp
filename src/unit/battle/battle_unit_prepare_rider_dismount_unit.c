#include "fft/battle_unit.h"

s32 battle_unit_prepare_rider_dismount(u32 runtime_id, s32 direction, s32 jump_length) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        unit->path.count = 1;
        unit->walk_speed = 0x2000;
        unit->step_speed = 0x2000;
        unit->path.pending_mount = 0;
        unit->movement_path_offset = 0;
        unit->path.height_offsets.source_height_offset = 0;
        unit->path.height_offsets.source_present = 0;
        unit->path.height_offsets.destination_height_offset = 0;
        unit->path.height_offsets.destination_present = 0;
        unit->path.data.steps[0].length = jump_length;
        unit->path.data.steps[0].slow = 0;
        unit->path.data.steps[0].climb = 0;
        unit->path.data.steps[0].mount = 0;
        unit->path.data.steps[0].higher = unit->map_z;
        unit->path.data.steps[0].direction = direction;
        unit->path.flags = 0;
        battle_unit_dismount_rider_and_update_display(unit);
    } else {
        main_system_handle_pointer_exception(12);
        return 0;
    }
    return 1;
}

/* Seats one unit on another by unit id (battle_unit_mount_rider_onto_unit).
 * Returns 0 when either record is missing. No retail code calls it. */
s32 battle_unit_mount_by_runtime_ids(u32 rider_runtime_id, u32 mount_runtime_id) {
    battle_unit_t* rider;
    battle_unit_t* mount;

    rider = battle_unit_get_by_runtime_id(rider_runtime_id & 0xffff);
    mount = battle_unit_get_by_runtime_id(mount_runtime_id & 0xffff);
    if ((rider != 0) && (mount != 0)) {
        battle_unit_mount_rider_onto_unit(rider, mount);
        return 1;
    }
    return 0;
}

#include "fft/battle_unit.h"

/* Prepare a unit's one-step movement path and dismount it from its rider.
 *
 * Returns 1, or 0 after raising the pointer exception when the runtime record is
 * missing. The 0x11a bit-fields must be cleared before the path step is built:
 * that order fixes the target's constant-load schedule. */
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

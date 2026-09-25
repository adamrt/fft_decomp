#include "fft/battle.h"
#include "psx/types.h"

/* Battle twin of world_unit_check_facing_relative_to_camera: nonzero when the
 * unit's facing, relative to the camera yaw quadrant, is one of the six
 * "away from camera" directions 7..12. */
s32 battle_unit_check_facing_relative_to_camera(s32 unit_id) {
    s32 misc_id;
    s32 camera_facing;

    misc_id = battle_get_misc_id(unit_id);
    camera_facing = battle_camera_wrap_yaw_angle();
    camera_facing += 0x200;
    camera_facing &= 0xf00;
    /* Preserve the retail arithmetic shift after the masked camera angle. */
    camera_facing >>= 8;
    camera_facing += battle_unit_get_facing_nibble_by_misc_id(misc_id);
    camera_facing &= 0xf;
    camera_facing -= 7;
    return (u32)camera_facing < 6U;
}

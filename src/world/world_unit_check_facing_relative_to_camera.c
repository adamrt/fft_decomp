#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_unit_check_facing_relative_to_camera(s32 unit_id) {
    s32 misc_id;
    s32 camera_facing;

    misc_id = world_get_misc_id(unit_id);
    camera_facing = world_camera_wrap_yaw_angle();
    camera_facing += 0x200;
    camera_facing &= 0xf00;
    /* Preserve the retail arithmetic shift after the masked camera angle. */
    camera_facing >>= 8;
    camera_facing += get_current_facing_byte_from_misc_id(misc_id);
    camera_facing &= 0xf;
    camera_facing -= 7;
    return (u32)camera_facing < 6U;
}

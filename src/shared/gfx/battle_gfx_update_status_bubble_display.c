#include "fft/battle_gfx.h"

/* Draws the unit's status bubble when the unit is on screen and has one.
 *
 * Nothing is drawn unless the camera-relative position lies within x 0x61..0x19f
 * and y -0x1f..0x10f, the ordering-table depth within 1..0x17f,
 * sprite_graphic_trigger is set, the unit is not a mount carrying its rider and
 * status_bubble_active is set. */
void battle_gfx_update_status_bubble_display(battle_unit_t* unit) {
    DVECTOR screen_coords;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_coords.vx = unit->camera_relative_x;
    screen_coords.vy = unit->camera_relative_y;
    if (screen_coords.vx < 0x61 || screen_coords.vx > 0x19f) {
        return;
    }
    if (screen_coords.vy < -0x1f || screen_coords.vy > 0x10f) {
        return;
    }
    if ((unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    battle_gfx_draw_status_bubble(unit, &screen_coords);
}

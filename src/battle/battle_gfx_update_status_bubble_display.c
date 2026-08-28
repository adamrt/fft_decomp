#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_update_status_bubble_display(battle_unit_misc_data_t* unit) {
    u16 screen_coords[2];
    u16 screen_x;
    u16 screen_y;
    s32 screen_x_in_range;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_x = *(u16*)&unit->item_get_camera_x;
    screen_coords[0] = screen_x;
    screen_x_in_range = (u32)(screen_x - 0x61) < 0x13f;
    screen_y = *(u16*)&unit->item_get_camera_y;
    screen_coords[1] = screen_y;
    if (!screen_x_in_range) {
        return;
    }
    if ((u16)(screen_y + 0x1f) >= 0x12f) {
        return;
    }
    if ((u32)(unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->sprite_graphic_trigger == 0) {
        return;
    }
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    battle_gfx_draw_status_bubble(unit, screen_coords);
}

#include "fft/world.h"

/*
 * Step the 24-frame return from the formation portrait to the unit grid.
 *
 * This reverses world_formation_step_grid_transition: the frame counter runs
 * down from 24, sliding the selected unit from its rest point back to its
 * grid cell while the other rows close in. Returns 1 while animating and 0 on
 * the call after the last frame. The unused volatile array reproduces the
 * original 16-byte gap below positions. Compiled at -O1.
 */
s32 world_formation_step_grid_return_transition(s32 mode) {
    volatile world_menu_point_t unused[4];
    world_menu_point_t positions[20];
    u8 frame;
    s32 spread;
    s32 shade;
    s32 i;
    s32 x;
    s32 y;
    s32 row_y;
    s32 target_x;
    s32 target_y;
    s32 direction;

    if (g_world_formation_grid_return_done != 0) {
        g_world_formation_grid_return_done = 0;
        return 0;
    }
    if (g_world_formation_grid_return_frame == 0) {
        world_formation_init_selection_cursor(g_world_formation_selected_unit_index);
        g_world_formation_grid_return_done = 0;
        g_world_formation_grid_return_frame = 24;
    }
    frame = g_world_formation_grid_return_frame;
    spread = (frame * frame) >> 1;
    shade = 0x80 - ((frame - 1) << 7) / 24;
    for (i = 0; i < g_world_formation_unit_count; i++) {
        row_y = (i / 4) * 60 + 0x24;
        y = g_world_formation_scroll_position + row_y;
        x = (i % 4) * 62 + 6;
        if (i == g_world_formation_selected_unit_index) {
            if (mode < 2) {
                target_x = 0x8B;
                target_y = 0x9E;
            } else {
                target_x = 0x61;
                target_y = 0x63;
            }
            positions[i].x
                = (x * (24 - g_world_formation_grid_return_frame) + target_x * g_world_formation_grid_return_frame)
                / 24;
            positions[i].y
                = (y * (24 - g_world_formation_grid_return_frame) + target_y * g_world_formation_grid_return_frame)
                / 24;
            g_world_formation_cursor_position.x = positions[i].x + 0x1C;
            g_world_formation_cursor_position.y = positions[i].y + 0x19;
        } else {
            if (i < 4) {
                direction = -1;
            } else if (i < 8) {
                direction = 1;
            } else if (i < 12) {
                direction = -1;
            } else {
                direction = 1;
            }
            positions[i].x = x + spread * direction;
            positions[i].y = y;
        }
    }
    g_world_formation_grid_return_frame--;
    if (g_world_formation_grid_return_frame == 0) {
        g_world_formation_grid_return_done = 1;
    }
    world_formation_draw_unit_slots(positions, shade);
    return 1;
}

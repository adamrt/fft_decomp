#include "fft/world.h"

/* Steps the 24-frame formation-grid transition and draws it.
 *
 * The selected unit slides from its grid cell to the portrait rest point
 * (0x8b,0x9e; 0x61,0x63 when mode >= 2) and drags the cursor with it; the
 * other units drift sideways by frame^2/2 (alternating direction per row)
 * while the backdrop shade fades. Returns the frame counter, which wraps to
 * 0 and clears g_world_formation_scroll_enabled when the transition ends. Compiled at -O1. */
u8 world_formation_step_grid_transition(s32 mode) {
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

    frame = g_world_formation_grid_transition_frame;
    spread = (frame * frame) >> 1;
    shade = 0x80 - (frame << 7) / 24;
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
            positions[i].x = (x * (24 - g_world_formation_grid_transition_frame)
                                 + target_x * g_world_formation_grid_transition_frame)
                / 24;
            positions[i].y = (y * (24 - g_world_formation_grid_transition_frame)
                                 + target_y * g_world_formation_grid_transition_frame)
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
    g_world_formation_grid_transition_frame++;
    if (g_world_formation_grid_transition_frame == 24) {
        g_world_formation_grid_transition_frame = 0;
        g_world_formation_scroll_enabled = 0;
    }
    world_formation_draw_unit_slots(positions, shade);
    return g_world_formation_grid_transition_frame;
}

#include "fft/world.h"
#include "psx/types.h"

/* Interpolates the sprite position from (0x61, 0x63) to (0x8b, 0x9e) over 24
 * steps and advances the step counter, wrapping back to zero. */
u8 world_formation_step_unit_portrait_slide(void) {
    s16 pos[2];
    s32 to_x = 0x8b;
    s32 to_y = 0x9e;
    s32 from_x = 0x61;
    s32 from_y = 0x63;
    s32 step;
    s32 remaining;

    step = g_world_formation_portrait_slide_frame;
    remaining = 24 - step;
    pos[0] = (remaining * from_x + step * to_x) / 24;
    pos[1] = (remaining * from_y + step * to_y) / 24;
    g_world_formation_cursor_position.x = pos[0] + 0x1c;
    g_world_formation_cursor_position.y = pos[1] + 0x19;
    world_formation_draw_unit_sprite(g_world_formation_selected_unit_index, (world_menu_point_t*)pos, (s8*)0, 0x32);
    g_world_formation_portrait_slide_frame++;
    if (g_world_formation_portrait_slide_frame == 24) {
        g_world_formation_portrait_slide_frame = 0;
    }
    return g_world_formation_portrait_slide_frame;
}

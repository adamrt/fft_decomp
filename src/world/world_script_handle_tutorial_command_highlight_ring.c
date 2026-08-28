#include "fft/world.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Tutorial command: lay out the highlight ring. The two corner arrays the
 * draw routine consumes are filled with 30 samples of a circle centred on
 * the command's (x, y) operand bytes, at three radii -- outer (r + 8),
 * middle (r + 4) and inner (r) -- each scaled independently in x and y by
 * the command's two scale bytes. Quad i of the first array spans outer to
 * middle, quad i of the second spans middle to inner, so the pair paints an
 * annulus. Entry 30 repeats entry 0 to close the ring. Operands are five
 * bytes after the opcode, so the command pointer advances by six.
 *
 * The step is 0x88 and the phase bias is 1024 (ONE / 4), i.e. a quarter
 * turn on the GTE's ONE-unit circle, so sample 0 starts at the top.
 */
void world_script_handle_tutorial_command_highlight_ring(void) {
    s32 center_x;
    s32 outer_x;
    s32 outer_y;
    s32 middle_x;
    s32 middle_y;
    s32 inner_x;
    s32 inner_y;
    s32 scale_x;
    s32 scale_y;
    s32 unused;
    s32 i;
    s32 step;

    step = 0x88;
    center_x = g_world_script_tutorial_command_ptr[3];
    scale_x = g_world_script_tutorial_command_ptr[4];
    scale_y = g_world_script_tutorial_command_ptr[5];
    for (i = 0; i < 30; i++) {
        outer_x = (center_x + 8) * scale_x;
        outer_y = (center_x + 8) * scale_y;
        middle_x = (center_x + 4) * scale_x;
        middle_y = (center_x + 4) * scale_y;
        inner_x = center_x * scale_x;
        inner_y = center_x * scale_y;
        g_world_tutorial_highlight_corners_a[i][0]
            = ((rcos(step * i + 1024) * outer_x) >> 19) + g_world_script_tutorial_command_ptr[1];
        g_world_tutorial_highlight_corners_a[i][1]
            = ((rsin(step * i + 1024) * outer_y) >> 19) + g_world_script_tutorial_command_ptr[2];
        g_world_tutorial_highlight_corners_a[i][2]
            = ((rcos(step * i + 1024) * middle_x) >> 19) + g_world_script_tutorial_command_ptr[1];
        g_world_tutorial_highlight_corners_a[i][3]
            = ((rsin(step * i + 1024) * middle_y) >> 19) + g_world_script_tutorial_command_ptr[2];
        g_world_tutorial_highlight_corners_b[i][0] = g_world_tutorial_highlight_corners_a[i][2];
        g_world_tutorial_highlight_corners_b[i][1] = g_world_tutorial_highlight_corners_a[i][3];
        g_world_tutorial_highlight_corners_b[i][2]
            = ((rcos(step * i + 1024) * inner_x) >> 19) + g_world_script_tutorial_command_ptr[1];
        g_world_tutorial_highlight_corners_b[i][3]
            = ((rsin(step * i + 1024) * inner_y) >> 19) + g_world_script_tutorial_command_ptr[2];
    }

    for (i = 0; i < 4; i++) {
        g_world_tutorial_highlight_corners_a[30][i] = g_world_tutorial_highlight_corners_a[0][i];
        g_world_tutorial_highlight_corners_b[30][i] = g_world_tutorial_highlight_corners_b[0][i];
    }

    g_world_script_tutorial_wait_timer = 2;
    g_world_script_tutorial_command_ptr += 6;
    g_world_tutorial_highlight_quad_count = 0;
}

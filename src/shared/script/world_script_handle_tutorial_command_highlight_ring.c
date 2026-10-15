#include "fft/world.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Tutorial command: lay out the highlight ring. The two corner arrays the
 * draw routine consumes are filled with 30 samples of a circle centerd on
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
/* Address-of operand fields preserves the -O0 byte-address steps. */
void world_script_handle_tutorial_command_highlight_ring(void) {
    s32 radius;
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
    radius = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->radius);
    scale_x = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->scale_x);
    scale_y = *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->scale_y);
    for (i = 0; i < 30; i++) {
        outer_x = (radius + 8) * scale_x;
        outer_y = (radius + 8) * scale_y;
        middle_x = (radius + 4) * scale_x;
        middle_y = (radius + 4) * scale_y;
        inner_x = radius * scale_x;
        inner_y = radius * scale_y;
        g_world_tutorial_highlight_corners_a[i][0].x = ((rcos(step * i + 1024) * outer_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_a[i][0].y = ((rsin(step * i + 1024) * outer_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
        g_world_tutorial_highlight_corners_a[i][1].x = ((rcos(step * i + 1024) * middle_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_a[i][1].y = ((rsin(step * i + 1024) * middle_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
        g_world_tutorial_highlight_corners_b[i][0].x = g_world_tutorial_highlight_corners_a[i][1].x;
        g_world_tutorial_highlight_corners_b[i][0].y = g_world_tutorial_highlight_corners_a[i][1].y;
        g_world_tutorial_highlight_corners_b[i][1].x = ((rcos(step * i + 1024) * inner_x) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->x);
        g_world_tutorial_highlight_corners_b[i][1].y = ((rsin(step * i + 1024) * inner_y) >> 19)
            + *(&((world_tutorial_circle_instruction_t*)g_world_script_tutorial_command_ptr)->y);
    }

    /* Point copies emit 32-bit transfers; retail copies the closing sample as four 16-bit values. */
    for (i = 0; i < 4; i++) {
        (&g_world_tutorial_highlight_corners_a[30][0].x)[i] = (&g_world_tutorial_highlight_corners_a[0][0].x)[i];
        (&g_world_tutorial_highlight_corners_b[30][0].x)[i] = (&g_world_tutorial_highlight_corners_b[0][0].x)[i];
    }

    g_world_script_tutorial_wait_timer = 2;
    g_world_script_tutorial_command_ptr += 6;
    g_world_tutorial_highlight_quad_count = 0;
}

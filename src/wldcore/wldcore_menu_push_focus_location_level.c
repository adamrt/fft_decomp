#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Flags word of the projection-motion record described in
 * src/wldcore/wldcore_map_start_projection_motion_if_outside_bounds.c. */

enum {
    WLDCORE_PROJECTION_MOTION_ACTIVE = 0x01,
};

/* The dot record's +0x10 render view. The target anchors the argument pair on
 * g_wldcore_map_dots + 0x10 and reaches screen_x as that anchor + 0x18, rather than
 * folding the offset into the global symbol. This local view retains that
 * target-proven related-address form while the shared record's flat layout
 * remains the established cross-module interface.
 */
typedef struct wldcore_map_dot_render {
    s32 flags;      /* dot +0x10 */
    s32 sprite_id;  /* dot +0x14 */
    s32 priority;   /* dot +0x18 */
    s32 unknown_0c; /* dot +0x1c */
    s32 unknown_10; /* dot +0x20 */
    s32 unknown_14; /* dot +0x24 */
    s32 screen_x;   /* dot +0x28 */
    s32 screen_y;   /* dot +0x2c */
    u8 rgb[3];      /* dot +0x30 */
    u8 pad2;
} wldcore_map_dot_render_t;

/* Pushes a map-dot menu level (type 0x32): records the dot index in the new
 * level, starts the wrapped projection move when the dot is off screen and,
 * while that move runs, flags the parent level's window record so it hides.
 *
 * Needs aspsx 2.21 (the module default); 2.34 emits the three-instruction $at
 * expansion for the indexed global stores and is 6 instructions short.
 *
 * The two call arguments carry the whole shape. The target materialises
 * g_wldcore_map_dots + 0x10 once, derives the motion point as that pointer + 0x18 and
 * the coordinate pair as the same constant - 8 (cse's related-value form).
 * Holding a single `wldcore_map_dot_t* dot` instead lets plus_constant fold
 * both displacements into the symbol, which costs the `addiu a0,a0,24`.
 * Anchoring the first argument on the +0x10 sub-record and spelling the second
 * as a direct array index restores both.
 */
void wldcore_menu_push_focus_location_level(s32 dot_index) {
    wldcore_map_dot_render_t* render;

    render = (wldcore_map_dot_render_t*)&g_wldcore_map_dots[dot_index].flags;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = dot_index;
    wldcore_map_start_projection_motion_if_outside_bounds(
        (wldcore_point32_t*)&render->screen_x, &g_wldcore_map_dots[dot_index].position.vx);
    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        g_main_system_flags |= 0x2000;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index].flags
            |= 0x10;
    }
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_FOCUS_LOCATION;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

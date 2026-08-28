#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/gte.h"

/* Places the player's map marker on EVENT_SCRIPT_VAR_LOCATION, queues it for
 * drawing, picks its animation from EVENT_SCRIPT_VAR_TOWN_BACKGROUND, then
 * centres the projection on it and refreshes the camera.
 *
 * The map-dot position is copied as one SVECTOR, which reproduces the
 * target's unaligned lwl/lwr word copy. */
void wldcore_map_init_location_marker_and_camera(void) {
    VECTOR unused; /* unreferenced; reproduces the target's 0x38-byte frame */
    wldcore_map_dot_t* marker;
    SVECTOR* position;
    s32 index;
    s32 count;
    s32 value;

    index = world_script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
    /* Only the kind, position and flags go through the pointer, as in the
     * target; the other fields are addressed through the symbol. */
    marker = &g_wldcore_map_projection_state.marker;
    marker->kind = index;
    position = &g_wldcore_map_dots[index].position;
    marker->position = *position;
    g_wldcore_map_projection_state.angle = 0;
    g_wldcore_map_projection_state.flags = 3;
    count = g_wldcore_window_render_object_count;
    g_wldcore_window_render_object_queue[count++] = (u32*)&marker->flags;
    marker->flags = 1;
    g_wldcore_map_projection_state.marker.priority = 0xE;
    g_wldcore_window_render_object_count = count;
    g_wldcore_map_projection_state.marker.anim_counter = 0;
    g_wldcore_map_projection_state.marker.frame_index = 0;
    g_wldcore_map_projection_state.marker.rgb[0] = 0x80;
    g_wldcore_map_projection_state.marker.rgb[1] = 0x80;
    g_wldcore_map_projection_state.marker.rgb[2] = 0x80;
    value = world_script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    g_wldcore_map_projection_state.marker.palette = 0;
    if (value == 1) {
        g_wldcore_map_projection_state.marker.palette = 2;
    }
    if (value == 2) {
        g_wldcore_map_projection_state.marker.palette = 3;
    }
    marker->position = g_wldcore_map_dots[marker->kind].position;
    wldcore_map_negate_and_clamp_coordinates(&g_wldcore_map_projection_state.marker.position.vx,
        (wldcore_map_clamped_point32_t*)&g_wldcore_map_projection_origin);
    wldcore_map_project_and_cull_tiles();
    wldcore_map_update_camera_direction_and_projection();
}

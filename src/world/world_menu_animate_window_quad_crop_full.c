#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Twin of world_menu_animate_window_quad_crop without the two-pixel right
 * inset. Animate the current window's first quad growing out from the rectangle
 * centre: each frame crops a sprite of the rectangle by the level's
 * percentage and copies the cropped bounds into the quad's corners.
 */
void world_menu_animate_window_quad_crop_full(menu_window_buffer_t* buffers, RECT* rect) {
    SPRT sprite;
    void* source; /* never assigned; the target passes an uninitialised $s5 */
    s32 level;
    u8 u = buffers->quads[0].u0;
    u8 v = buffers->quads[0].v0;
    s32 unused[2]; /* two unreferenced locals account for the 0x50 frame */

    for (level = 0; level < 12; level += g_world_event_speed) {
        sprite.u0 = u;
        sprite.v0 = v;
        sprite.x0 = rect->x;
        sprite.y0 = rect->y;
        sprite.w = rect->w;
        sprite.h = rect->h;
        world_menu_zoom_cursor_frame(source, &sprite, level);
        buffers[g_world_menu_packet_buffer_index].quads[0].x0 = sprite.x0;
        buffers[g_world_menu_packet_buffer_index].quads[0].y0 = sprite.y0;
        buffers[g_world_menu_packet_buffer_index].quads[0].x1 = sprite.x0 + sprite.w;
        buffers[g_world_menu_packet_buffer_index].quads[0].y1 = sprite.y0;
        buffers[g_world_menu_packet_buffer_index].quads[0].x2 = sprite.x0;
        buffers[g_world_menu_packet_buffer_index].quads[0].y2 = sprite.y0 + sprite.h;
        buffers[g_world_menu_packet_buffer_index].quads[0].x3 = sprite.x0 + sprite.w;
        buffers[g_world_menu_packet_buffer_index].quads[0].y3 = sprite.y0 + sprite.h;
        buffers[g_world_menu_packet_buffer_index].quads[0].u0 = sprite.u0;
        buffers[g_world_menu_packet_buffer_index].quads[0].v0 = sprite.v0;
        buffers[g_world_menu_packet_buffer_index].quads[0].u1 = sprite.u0 + sprite.w;
        buffers[g_world_menu_packet_buffer_index].quads[0].v1 = sprite.v0;
        buffers[g_world_menu_packet_buffer_index].quads[0].u2 = sprite.u0;
        buffers[g_world_menu_packet_buffer_index].quads[0].v2 = sprite.v0 + sprite.h;
        buffers[g_world_menu_packet_buffer_index].quads[0].u3 = sprite.u0 + sprite.w;
        buffers[g_world_menu_packet_buffer_index].quads[0].v3 = sprite.v0 + sprite.h;
        world_thread_yield();
    }
}

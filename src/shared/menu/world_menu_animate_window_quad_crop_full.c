#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Twin of menu_animate_window_quad_crop without the two-pixel right
 * inset. Animate the current window's first quad growing out from the rectangle
 * center: each frame crops a sprite of the rectangle by the level's
 * percentage and copies the cropped bounds into the quad's corners.
 */
void world_menu_animate_window_quad_crop_full(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to) {
    SPRT sprite;
    void* source; /* never assigned; the target passes an uninitialized $s5 */
    s32 level;
    u8 u = buffers->quads[0].u0;
    u8 v = buffers->quads[0].v0;
    s32 unused[2]; /* two unreferenced locals account for the 0x50 frame */

    for (level = 0; level < 12; level += g_script_event_speed) {
        sprite.u0 = u;
        sprite.v0 = v;
        sprite.x0 = rect->x;
        sprite.y0 = rect->y;
        sprite.w = rect->w;
        sprite.h = rect->h;
        menu_zoom_cursor_frame(source, &sprite, level);
        setXY4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.x0, sprite.y0, sprite.x0 + sprite.w, sprite.y0,
            sprite.x0, sprite.y0 + sprite.h, sprite.x0 + sprite.w, sprite.y0 + sprite.h);
        setUV4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.u0, sprite.v0, sprite.u0 + sprite.w, sprite.v0,
            sprite.u0, sprite.v0 + sprite.h, sprite.u0 + sprite.w, sprite.v0 + sprite.h);
        thread_yield();
    }
}

#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Flip the frame buffers and repaint the background of the new draw buffer.
 *
 * Applies the other buffer's draw and display environments. Unless a movie is
 * streaming, the 256- or 320-pixel-wide frame is then cleared
 * (OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME), overwritten with the 512x240 picture at
 * VRAM (512, 256) (OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM, the ending script)
 * or uploaded from the frame image at g_open_file_destination. The display is
 * enabled unless OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED is set.
 *
 * Nothing in OPEN.BIN sets OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME. */
void open_movie_present_frame(void) {
    RECT image_rect;
    RECT move_rect;
    s32 display_index;
    u32 flags;

    display_index = g_active_graphics_buffer_index ^ 1;
    g_active_graphics_buffer_index = display_index;
    PutDrawEnv(&g_open_gfx_screen_environments[display_index].draw);

    display_index = g_active_graphics_buffer_index;
    PutDispEnv(&g_open_gfx_screen_environments[display_index].disp);

    flags = g_open_system_runtime_flags;
    if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        image_rect.h = 240;
        image_rect.x = 0;
        if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) != 0) {
            image_rect.w = 320;
        } else {
            image_rect.w = 256;
        }
        if ((g_active_graphics_buffer_index & 1) != 0) {
            image_rect.y = 240;
        } else {
            image_rect.y = 0;
        }

        flags = g_open_system_runtime_flags;
        if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_CLEAR_FRAME) != 0) {
            ClearImage(&image_rect, 0, 0, 0);
        } else if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM) != 0) {
            move_rect.x = 512;
            move_rect.y = 256;
            move_rect.w = 512;
            move_rect.h = 240;
            MoveImage(&move_rect, image_rect.x, image_rect.y);
        } else {
            LoadImage(&image_rect, g_open_file_destination);
        }
    }

    SetDispMask(((g_open_system_runtime_flags >> 6) ^ 1) & 1);
}

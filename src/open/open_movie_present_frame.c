#include "fft/open.h"

void open_movie_present_frame(void) {
    RECT image_rect;
    RECT move_rect;
    s32 display_index;
    u32 flags;

    display_index = g_main_gfx_display_buffer_index ^ 1;
    g_main_gfx_display_buffer_index = display_index;
    PutDrawEnv(&g_open_gfx_screen_environments[display_index].draw);

    display_index = g_main_gfx_display_buffer_index;
    PutDispEnv(&g_open_gfx_screen_environments[display_index].disp);

    flags = g_open_system_runtime_flags;
    if ((flags & 2) == 0) {
        image_rect.h = 240;
        image_rect.x = 0;
        if ((flags & OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN) != 0) {
            image_rect.w = 320;
        } else {
            image_rect.w = 256;
        }
        if ((g_main_gfx_display_buffer_index & 1) != 0) {
            image_rect.y = 240;
        } else {
            image_rect.y = 0;
        }

        flags = g_open_system_runtime_flags;
        if ((flags & 0x20000) != 0) {
            ClearImage(&image_rect, 0, 0, 0);
        } else if ((flags & 0x40000) != 0) {
            move_rect.x = 512;
            move_rect.y = 256;
            move_rect.w = 512;
            move_rect.h = 240;
            MoveImage(&move_rect, image_rect.x, image_rect.y);
        } else {
            LoadImage(&image_rect, (u32*)g_open_file_destination);
        }
    }

    SetDispMask(((g_open_system_runtime_flags >> 6) ^ 1) & 1);
}

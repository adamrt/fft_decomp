#include "fft/jobstts.h"

u8* jobstts_cmd_draw_textured_quad_handler(u8* data) {
    urect16_t rect;
    s32 y;
    u8* pal;

    if (g_jobstts_menu_list_row_rendering == 0) {
        y = data[4];
    } else {
        y = (((s16)g_jobstts_menu_list_row_height * g_jobstts_menu_list_row_index) + data[4])
            - g_jobstts_menu_list_scroll_offset_y;
        if (g_jobstts_menu_list_scroll_offset_y < 0) {
            y -= (s16)g_jobstts_menu_list_row_height;
        }
    }
    rect.x = data[3];
    rect.y = y;
    rect.w = data[5];
    rect.h = data[6];
    pal = g_jobstts_text_color;
    if (data[0] == 4) {
        pal = 0;
    }
    /* Preserve the caller's promoted ABI arguments; the callee truncates its u16 parameters. */
    ((void (*)(urect16_t*, u8, u8, u8*, s32, s32, s32, s32))jobstts_gfx_enqueue_textured_quad)(&rect, data[7], data[8],
        pal, g_jobstts_gfx_semitransparency, g_jobstts_gfx_texture_page, g_jobstts_gfx_clut_id,
        g_jobstts_gfx_otag_index);
    return data + data[1];
}

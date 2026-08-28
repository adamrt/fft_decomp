#include "fft/jobstts.h"
#include "psx/types.h"

void jobstts_text_set_palette_and_metrics(s32 mode) {
    g_jobstts_text_layout_mode = mode;
    if (mode != 0) {
        g_jobstts_text_metric_0 = g_jobstts_text_clut_0_mode1;
        g_jobstts_text_metric_1 = g_jobstts_text_clut_1_mode1;
        g_jobstts_gfx_background_clut_id = g_jobstts_text_clut_2_mode1;
        g_jobstts_text_metric_3 = g_jobstts_text_clut_3_mode1;
        g_jobstts_text_metric_4 = g_jobstts_menu_cursor_mode1_foreground_clut;
        g_jobstts_text_metric_5 = g_jobstts_menu_cursor_mode1_background_clut;
        g_jobstts_text_color[0] = 0x60;
        g_jobstts_text_color[1] = 0x60;
        g_jobstts_text_color[2] = 0x80;
    } else {
        g_jobstts_text_metric_0 = g_jobstts_text_clut_0_mode0;
        g_jobstts_text_metric_1 = g_jobstts_text_clut_1_mode0;
        g_jobstts_gfx_background_clut_id = g_jobstts_text_clut_2_mode0;
        g_jobstts_text_metric_3 = g_jobstts_text_clut_3_mode0;
        g_jobstts_text_metric_4 = g_jobstts_menu_cursor_mode0_foreground_clut;
        g_jobstts_text_metric_5 = g_jobstts_menu_cursor_mode0_background_clut;
        g_jobstts_text_color[0] = 0x80;
        g_jobstts_text_color[1] = 0x80;
        g_jobstts_text_color[2] = 0x80;
    }
}

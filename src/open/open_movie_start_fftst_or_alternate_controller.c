#include "fft/open.h"
#include "psx/types.h"

void open_movie_start_fftst_or_alternate_controller(s32 use_alternate) {
    g_open_current_controller_index = 0;
    if (use_alternate == 0) {
        g_main_gfx_display_buffer_index = 0;
        open_movie_stream_fftst_and_push_controller(0x153D6, 0x278A, 0x3DF, 0x2DE);
    } else {
        open_title_push_menu_controller(0);
    }
}

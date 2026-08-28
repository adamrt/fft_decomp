#include "fft/open.h"
#include "psx/types.h"

void open_movie_stream_and_push_controller_1(s32 start_sector, s32 unused, s32 end_frame) {
    s32 controller;

    (void)unused;
    open_movie_start_stream(start_sector, end_frame, -1, 0x94);
    g_open_system_runtime_flags &= ~0x40;
    controller = g_open_current_controller_index;
    g_open_controller_handler_indices[controller] = 1;
    g_open_current_controller_index = controller + 1;
}

/* This variant starts handler state one and uses movie rate 0x94. */

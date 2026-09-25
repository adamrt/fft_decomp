#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct open_movie_stream_controller_state {
    s32 start_sector;
    s32 stream_length;
} open_movie_stream_controller_state_t;

void open_movie_finish_stream_controller_if_ready_or_skipped(const open_movie_stream_controller_state_t* state) {
    if (g_open_system_runtime_flags & 2) {
        if (!(g_open_input_new_button_presses & PSX_PAD_START)) {
            return;
        }

        open_movie_stop_stream(0);
        SetDispMask(0);
        g_open_system_runtime_flags |= 0x40;
    }

    g_open_current_controller_index--;
    open_title_push_menu_controller(state->start_sector + state->stream_length);
}

/* An active movie waits for Start; a completed movie advances directly. */
/* The two state words locate the disc sector immediately after the stream. */

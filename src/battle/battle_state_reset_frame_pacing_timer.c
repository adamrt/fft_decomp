#include "fft/battle_camera.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_reset_frame_pacing_timer(void) {
    g_frame_pacing_timer = 0;
}

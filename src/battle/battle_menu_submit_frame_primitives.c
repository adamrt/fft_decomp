#include "fft/battle_menu_window.h"

/* Submit the six primitives that compose one menu-frame buffer. */
void battle_menu_submit_frame_primitives(battle_menu_window_record_t* frame) {
    battle_gfx_draw_or_append_gpu_primitive(&frame->sprites[3]);
    battle_gfx_draw_or_append_gpu_primitive(&frame->sprites[1]);
    battle_gfx_draw_or_append_gpu_primitive(&frame->sprites[2]);
    battle_gfx_draw_or_append_gpu_primitive(&frame->mode1);
    battle_gfx_draw_or_append_gpu_primitive(&frame->sprites[0]);
    battle_gfx_draw_or_append_gpu_primitive(&frame->mode0);
}

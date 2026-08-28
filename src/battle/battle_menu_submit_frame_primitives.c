#include "fft/battle_gfx.h"

typedef struct battle_menu_frame_primitives {
    s32 primitive_0[3];
    s32 primitive_1[3];
    s32 primitive_2[5];
    s32 primitive_3[5];
    s32 primitive_4[5];
    s32 primitive_5[1];
} battle_menu_frame_primitives_t;

/* Submit the six primitives that compose one menu-frame buffer. */
void battle_menu_submit_frame_primitives(battle_menu_frame_primitives_t* frame) {
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_5);
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_3);
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_4);
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_1);
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_2);
    battle_gfx_draw_or_append_gpu_primitive(frame->primitive_0);
}

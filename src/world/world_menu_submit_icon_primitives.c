#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_menu_icon_prims_t {
    /* 0x00 */ DR_MODE mode[2];
    /* 0x18 */ SPRT sprites[4];
} world_menu_icon_prims_t;

void world_menu_submit_icon_primitives(world_menu_icon_prims_t* prims) {
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[3]);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[1]);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[2]);
    world_gfx_draw_or_append_gpu_primitive(&prims->mode[1]);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[0]);
    world_gfx_draw_or_append_gpu_primitive(&prims->mode[0]);
}

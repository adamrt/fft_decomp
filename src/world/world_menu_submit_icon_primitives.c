#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_menu_submit_icon_primitives(world_menu_icon_sprites_t* prims) {
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[3]);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[1]);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[2]);
    world_gfx_draw_or_append_gpu_primitive(&prims->draw_mode);
    world_gfx_draw_or_append_gpu_primitive(&prims->sprites[0]);
    world_gfx_draw_or_append_gpu_primitive(&prims->first_draw_mode);
}

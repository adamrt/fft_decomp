#include "fft/world.h"
#include "psx/types.h"

void world_gfx_submit_primitive_group(world_primitive_group_t* group) {
    s32 i;

    for (i = 0; i < 12; i++) {
        world_gfx_draw_or_append_gpu_primitive(group->prims[i]);
    }
    world_gfx_draw_or_append_gpu_primitive(group->header);
    world_gfx_draw_or_append_gpu_primitive(&group->tail);
    world_gfx_draw_or_append_gpu_primitive(group->footer);
}

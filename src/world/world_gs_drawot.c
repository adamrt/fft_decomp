#include "psx/types.h"

typedef struct {
    u8 pad[0x10];
    u32 ot;
} world_gs_draw_ot_arg_t;

void world_gs_drawot(world_gs_draw_ot_arg_t* arg) {
    DrawOTag(arg->ot);
}

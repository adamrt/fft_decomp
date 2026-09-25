#include "fft/world.h"
#include "psx/types.h"
typedef struct {
    u8 lo;
    u8 hi;
} world_item_type_icon_uv_t;
extern world_item_type_icon_uv_t g_world_item_type_icon_rects[];
void world_item_get_type_icon_rect(s32 index, world_item_icon_source_t* out) {
    out->u = g_world_item_type_icon_rects[index].lo;
    out->v = g_world_item_type_icon_rects[index].hi;
    out->w = 0xC;
    out->h = 0xC;
}

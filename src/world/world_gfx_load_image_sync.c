#include "psx/gpu.h"
#include "psx/types.h"

void world_gfx_load_image_sync(RECT* rect, u32* data) {
    LoadImage(rect, data);
    while (DrawSync(1) != 0) { }
}

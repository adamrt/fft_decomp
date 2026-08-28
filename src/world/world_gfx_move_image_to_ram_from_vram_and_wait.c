#include "psx/gpu.h"
#include "psx/types.h"

void world_gfx_move_image_to_ram_from_vram_and_wait(RECT* rect, u32* dest) {
    StoreImage(rect, dest);
    while (DrawSync(1) != 0) { }
}

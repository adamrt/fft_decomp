#include "psx/gpu.h"

void equip_gfx_store_image_and_wait(RECT* rect, u32* data) {
    StoreImage(rect, data);
    while (DrawSync(1) != 0) { }
}

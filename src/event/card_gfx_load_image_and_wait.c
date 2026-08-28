#include "psx/gpu.h"

void card_gfx_load_image_and_wait(RECT* rect, u32* data) {
    LoadImage(rect, data);
    while (DrawSync(1) != 0) { }
}

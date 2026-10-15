#include "fft/gfx.h"

void gfx_load_image_and_wait(RECT* rect, void* data) {
    LoadImage(rect, data);
    while (DrawSync(1) != 0) { }
}

void gfx_store_image_and_wait(RECT* rect, void* data) {
    StoreImage(rect, data);
    while (DrawSync(1) != 0) { }
}

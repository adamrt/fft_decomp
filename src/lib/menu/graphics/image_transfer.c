/* LoadImage/StoreImage followed by a drawing wait. */

#include "fft/gfx_image.h"
#include "psx/libgpu.h"

/* Copy an image from main memory into a VRAM rectangle and wait for the transfer to finish. */
void gfx_load_image_and_wait(RECT* rect, void* data) {
    LoadImage(rect, data);
    while (DrawSync(1) != 0) { }
}

/* Copy a VRAM rectangle to main memory and wait for the transfer to finish. */
void gfx_store_image_and_wait(RECT* rect, void* data) {
    StoreImage(rect, data);
    while (DrawSync(1) != 0) { }
}

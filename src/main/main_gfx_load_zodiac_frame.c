#include "psx/gpu.h"

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * words at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void main_gfx_load_zodiac_frame(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image;

    image = main_file_get_bin_as_tim(0xec61, 0x10000);
    LoadImage(&destination, image);
    DrawSync(0);
    main_heap_free(image);
    main_gfx_load_frame_bin_into_vram();
}

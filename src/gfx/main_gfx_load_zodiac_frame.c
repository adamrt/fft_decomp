#include "fft/main.h"
#include "psx/libgpu.h"

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void main_gfx_load_zodiac_frame(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image;

    image = main_file_load_checked_to_heap(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);
    LoadImage(&destination, image);
    DrawSync(0);
    main_heap_free(image);
    main_gfx_load_frame_bin_into_vram();
}

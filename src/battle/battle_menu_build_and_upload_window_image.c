#include "fft/battle_menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* menu_build_window_image with keep 0: the image is uploaded to rect and
 * its buffer freed. */
void battle_menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset) {
    menu_build_window_image(width, height, rect, mode, tail_offset, 0);
}

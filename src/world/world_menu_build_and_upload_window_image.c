#include "fft/world.h"
#include "psx/types.h"

void world_menu_build_and_upload_window_image(s32 width, s32 height, s32 rect, s32 mode, s32 tail_offset) {
    world_menu_build_window_image(width, height, (RECT*)rect, mode, tail_offset, 0);
}

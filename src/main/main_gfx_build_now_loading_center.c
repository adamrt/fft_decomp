#include "psx/types.h"

/* Build the Now Loading message centred horizontally (x = 0x80). */
void main_gfx_build_now_loading_center(u32 visible) {
    main_gfx_build_now_loading(visible, 0x80, 0);
}

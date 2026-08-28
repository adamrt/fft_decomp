#include "fft/world.h"
#include "psx/types.h"

void* world_menu_alloc_buffer(s32 size) {
    return world_menu_alloc_ui_buffer(size);
}

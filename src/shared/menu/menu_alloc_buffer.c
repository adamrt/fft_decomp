#include "fft/menu.h"
#include "psx/types.h"

/* Thin public entry point for the BATTLE menu-buffer allocator. */
void* menu_alloc_buffer(s32 bytes) {
    return menu_alloc_memory(bytes);
}

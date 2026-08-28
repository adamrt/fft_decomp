#include "fft/battle.h"
#include "psx/types.h"

/* Thin public entry point for the BATTLE menu-buffer allocator. */
void* battle_menu_alloc_buffer(s32 bytes) {
    return battle_menu_alloc_memory(bytes);
}

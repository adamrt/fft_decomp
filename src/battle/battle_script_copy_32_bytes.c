#include "fft/world.h"
#include "psx/types.h"

void battle_script_copy_32_bytes(void* destination, const void* source) {
    battle_copy_bytes(destination, source, 0x20);
}

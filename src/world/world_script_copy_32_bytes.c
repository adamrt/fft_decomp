#include "fft/world.h"
#include "psx/types.h"

void world_script_copy_32_bytes(void* destination, const void* source) {
    world_script_copy_bytes(destination, source, 32);
}

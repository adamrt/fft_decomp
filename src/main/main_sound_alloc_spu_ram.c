#include "psx/spu.h"
#include "psx/types.h"

/* "SpuMalloc with extra steps": the sound code always reserves SPU RAM
 * through this wrapper, which now only forwards to the library. */
s32 main_sound_alloc_spu_ram(s32 size) {
    return SpuMalloc(size);
}

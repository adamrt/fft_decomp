#include "fft/suzuki.h"
#include "psx/libspu.h"

/* "SpuMalloc with extra steps": the sound code always reserves SPU RAM
 * through this wrapper, which now only forwards to the library. */
s32 suzuki_sound_alloc_spu_ram(s32 size) {
    return SpuMalloc(size);
}

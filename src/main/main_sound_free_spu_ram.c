#include "psx/spu.h"
#include "psx/types.h"

/* "SpuFree with extra steps": the sound code always releases SPU RAM
 * through this wrapper, which now only forwards to the library and reports
 * success. */
s32 main_sound_free_spu_ram(u32 addr) {
    SpuFree(addr);
    return 0;
}

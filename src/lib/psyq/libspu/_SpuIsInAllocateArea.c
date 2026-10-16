/* SCUS_942.21 0x80019f08..0x80019f87. */
#include "psx/libspu.h"

/* Return 1 when an allocated SPU block reaches into the range from byte address up to the top of
 * SPU RAM (a reverb work area), else 0; 0 also before SpuInitMalloc. */
s32 _SpuIsInAllocateArea(u32 address) {
    s32 index;
    u32 start;
    if (_spu_memList == 0) {
        return 0;
    }
    for (index = 0;; index++) {
        start = _spu_memList[index].address;
        if (start & PSYQ_SPU_HEAP_FREE) {
            continue;
        }
        if (start & PSYQ_SPU_HEAP_TAIL) {
            break;
        }
        start &= PSYQ_SPU_HEAP_ADDRESS_MASK;
        if (start >= address) {
            return 1;
        }
        if (address < start + _spu_memList[index].size) {
            return 1;
        }
    }
    return 0;
}

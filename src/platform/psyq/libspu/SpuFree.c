/* SCUS_942.21 0x80019ae0..0x80019b5b. */
#include "psx/libspu.h"

void SpuFree(u32 addr) {
    s32 index = 0;
    for (; index < _spu_AllocBlockNum; index++) {
        if (_spu_memList[index].address & PSYQ_SPU_HEAP_TAIL) {
            break;
        }
        if (_spu_memList[index].address == addr) {
            _spu_memList[index].address = addr | PSYQ_SPU_HEAP_FREE;
            break;
        }
    }
    _spu_gcSPU();
}

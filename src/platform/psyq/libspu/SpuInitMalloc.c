/* SCUS_942.21 0x800194c4..0x80019517. */
#include "psx/libspu.h"

s32 SpuInitMalloc(s32 num, char* top) {
    psyq_spu_heap_record_t* table = (psyq_spu_heap_record_t*)top;
    if (num > 0) {
        table[0].address = PSYQ_SPU_HEAP_TAIL | PSYQ_SPU_HEAP_START;
        _spu_memList = table;
        _spu_AllocLastNum = 0;
        _spu_AllocBlockNum = num;
        table[0].size = (0x10000 << _spu_mem_mode_plus) - PSYQ_SPU_HEAP_START;
        return num;
    }
    return 0;
}

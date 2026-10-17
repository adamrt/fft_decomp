/* SCUS_942.21 0x80019518..0x800197df. */
#include "psx/libspu.h"

s32 SpuMalloc(s32 size) {
    s32 i = 0;
    s32 found = -1;
    u32 reserved;
    s32 rounded;
    u32 free_size;
    if (_spu_rev_reserve_wa == 0) {
        reserved = 0;
    } else {
        reserved = (0x10000 - _spu_rev_offsetaddr) << _spu_mem_mode_plus;
    }
    rounded = (size & ~_spu_mem_mode_unitM) ? size + _spu_mem_mode_unitM : size;
    size = rounded >> _spu_mem_mode_plus;
    size <<= _spu_mem_mode_plus;
    if (_spu_memList[0].address & PSYQ_SPU_HEAP_TAIL) {
        found = 0;
    } else {
        _spu_gcSPU();
        for (; i < _spu_AllocBlockNum; i++) {
            if ((_spu_memList[i].address & PSYQ_SPU_HEAP_TAIL)
                || ((_spu_memList[i].address & PSYQ_SPU_HEAP_FREE) && _spu_memList[i].size >= size)) {
                found = i;
                break;
            }
        }
    }
    if (found != -1) {
        if (_spu_memList[found].address & PSYQ_SPU_HEAP_TAIL) {
            if (found < _spu_AllocBlockNum) {
                if (_spu_memList[found].size - reserved >= size) {
                    _spu_AllocLastNum = found + 1;
                    _spu_memList[_spu_AllocLastNum].address
                        = ((_spu_memList[found].address & PSYQ_SPU_HEAP_ADDRESS_MASK) + size) | PSYQ_SPU_HEAP_TAIL;
                    _spu_memList[_spu_AllocLastNum].size = _spu_memList[found].size - size;
                    _spu_memList[found].size = size;
                    _spu_memList[found].address &= PSYQ_SPU_HEAP_ADDRESS_MASK;
                    _spu_gcSPU();
                    return _spu_memList[found].address;
                }
            }
        } else {
            free_size = _spu_memList[found].size;
            if (size < free_size) {
                if (_spu_AllocLastNum < _spu_AllocBlockNum) {
                    u32 address;
                    u32 last_address;
                    u32 last_size;
                    address = size + _spu_memList[found].address;
                    last_address = _spu_memList[_spu_AllocLastNum].address;
                    last_size = _spu_memList[_spu_AllocLastNum].size;
                    _spu_memList[_spu_AllocLastNum].address = address | PSYQ_SPU_HEAP_FREE;
                    _spu_memList[_spu_AllocLastNum].size = free_size - size;
                    _spu_memList[_spu_AllocLastNum + 1].address = last_address;
                    _spu_memList[_spu_AllocLastNum + 1].size = last_size;
                    _spu_AllocLastNum++;
                }
            }
            _spu_memList[found].size = size;
            _spu_memList[found].address &= PSYQ_SPU_HEAP_ADDRESS_MASK;
            _spu_gcSPU();
            return _spu_memList[found].address;
        }
    }
    return -1;
}

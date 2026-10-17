/* SCUS_942.21 0x8001920c..0x800192af. */
#include "psx/libspu.h"

/* Round a byte address up to the SPU memory unit and store it, in units, in register reg_index;
 * the pseudo-indexes PSYQ_SPU_ADDRESS_AS_UNITS and PSYQ_SPU_ADDRESS_ALIGNED_BYTES only return it
 * in units or as rounded bytes. */
u32 _spu_FsetRXXa(s32 reg_index, u32 address) {
    u32 units;
    u16 unit_address;
    if (_spu_mem_mode && address % _spu_mem_mode_unit) {
        address += _spu_mem_mode_unit;
        address &= ~_spu_mem_mode_unitM;
    }
    units = address >> _spu_mem_mode_plus;
    switch (reg_index) {
    case PSYQ_SPU_ADDRESS_AS_UNITS:
        unit_address = units;
        return unit_address;
    case PSYQ_SPU_ADDRESS_ALIGNED_BYTES:
        return address;
    default:
        _spu_RXX->raw[reg_index] = units;
        return address;
    }
}

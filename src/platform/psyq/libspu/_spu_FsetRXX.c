/* SCUS_942.21 0x800191c4..0x8001920b. */
#include "psx/libspu.h"

/* Store value in SPU register reg_index, converting a byte address to SPU units unless is_address
 * is PSYQ_SPU_REGISTER_RAW. */
void _spu_FsetRXX(s32 reg_index, u32 value, s32 is_address) {
    if (is_address == PSYQ_SPU_REGISTER_RAW) {
        _spu_RXX->raw[reg_index] = value;
    } else {
        _spu_RXX->raw[reg_index] = value >> _spu_mem_mode_plus;
    }
}

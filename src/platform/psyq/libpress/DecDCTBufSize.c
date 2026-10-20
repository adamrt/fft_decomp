/* Return the run-level buffer size from the low 16 bits of the bitstream header. */

#include "psx/libpress.h"

s32 DecDCTBufSize(u32* bs) {
    return (u16)*bs;
}

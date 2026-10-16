#include "psx/libpress.h"

/* Move the body comment up as the leading comment: 'Return the run-level buffer size from the low
 * 16 bits of the bitstream's first 32-bit header value.'. */
s32 DecDCTBufSize(u32* bs) {
    /* The bitstream length is the low 16 bits of its first 32-bit header value. */
    return (u16)*bs;
}

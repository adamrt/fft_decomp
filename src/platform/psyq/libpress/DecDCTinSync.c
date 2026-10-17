#include "psx/libpress.h"

/* Poll for completion; unused_mode is ignored. */
s32 DecDCTinSync(s32 unused_mode) {
    return MDEC_in_sync();
}

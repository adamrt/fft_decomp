#include "psx/libpress.h"

/* Poll for completion; unused_mode is ignored. */
s32 DecDCToutSync(s32 unused_mode) {
    return MDEC_out_sync();
}

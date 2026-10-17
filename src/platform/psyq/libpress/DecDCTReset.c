#include "psx/libetc.h"
#include "psx/libpress.h"

/* Resets the MDEC; mode 0 also resets the callback table first. */
void DecDCTReset(s32 mode) {
    if (mode == 0) {
        ResetCallback();
    }
    MDEC_reset(mode);
}

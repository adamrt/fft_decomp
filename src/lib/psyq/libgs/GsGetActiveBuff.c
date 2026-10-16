#include "psx/libgs.h"
#include "psx/types.h"

/* Return the index (0 or 1) of the buffer currently being drawn. */
s32 GsGetActiveBuff(void) {
    return PSDIDX;
}

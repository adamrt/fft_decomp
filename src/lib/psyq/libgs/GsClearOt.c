#include "psx/libgs.h"
#include "psx/types.h"

/* Reset an ordering table: record its offset and point value, aim its head tag at the last entry
 * and clear the 2^length entries in reverse order. */
void GsClearOt(u16 x, u16 y, GsOT* ot) {
    ot->offset = x;
    ot->point = y;
    ot->tag = ot->org + (1 << ot->length) - 1;
    ClearOTagR((u32*)ot->org, 1 << ot->length);
}

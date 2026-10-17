#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d158–0x8001d164. */
s32 ReadGeomScreen(void) {
    s32 screen;
    PSYQ_GTE_CFC2(PSYQ_GTE_CTRL_H, screen);
    return screen;
}

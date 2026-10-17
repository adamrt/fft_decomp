#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d1c8–0x8001d1d4. */
void SetGeomScreen(int screen) {
    PSYQ_GTE_CTC2(PSYQ_GTE_CTRL_H, screen);
}

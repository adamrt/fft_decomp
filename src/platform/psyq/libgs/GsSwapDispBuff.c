#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Present the active buffer and flip.
 *
 * The frame counter is pre-incremented in place and its new value stored
 * again after skipping zero; the pre-increment keeps the target's register
 * copy between the increment and the first store. */
void GsSwapDispBuff(void) {
    u32 stored;

    GsDISPENV.disp.x = PSDOFSX[PSDIDX];
    GsDISPENV.disp.y = PSDOFSY[PSDIDX];
    PutDispEnv(&GsDISPENV);
    SetDispMask(1);

    stored = ++PSDCNT;
    if (stored == 0) {
        stored = 1;
    }
    PSDCNT = stored;
    PSDIDX = PSDIDX == 0;
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

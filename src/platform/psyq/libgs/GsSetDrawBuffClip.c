#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Program the active draw buffer's clip rectangle: the shared clip rectangle
 * shifted by that buffer's offsets. */
void GsSetDrawBuffClip(void) {
    s32 x;
    s32 y;

    x = CLIP2.x + PSDOFSX[PSDIDX];
    y = CLIP2.y + PSDOFSY[PSDIDX];
    setRECT(&GsDRAWENV.clip, x, y, CLIP2.w, CLIP2.h);
    PutDrawEnv(&GsDRAWENV);
}

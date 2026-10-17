#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Applies the current draw buffer's offsets. In offset draw mode the offset
 * moves into the draw environment and the GTE offsets are cleared; otherwise
 * the *other* buffer's offsets are programmed into the GTE and cached. */
void GsSetDrawBuffOffset(void) {
    s32 x;
    s32 y;

    if (PSDGPU != 0) {
        GsORGOFSX = GsORGOFSY = 0;
        GsDRAWENV.ofs[0] = POSITION.offx + PSDOFSX[PSDIDX];
        GsDRAWENV.ofs[1] = POSITION.offy + PSDOFSY[PSDIDX];
        PutDrawEnv(&GsDRAWENV);
    } else {
        x = POSITION.offx + PSDOFSX[!PSDIDX];
        y = POSITION.offy + PSDOFSY[!PSDIDX];
        SetGeomOffset(x, y);
        GsORGOFSX = x;
        GsORGOFSY = y;
    }
}

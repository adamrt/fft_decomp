#include "psx/libgpu.h"
#include "psx/libgs.h"

/* Copy the clip rectangle that the draw environment setup reads. */
void GsSetClip2D(RECT* rectp) {
    setRECT(&CLIP2, rectp->x, rectp->y, rectp->w, rectp->h);
}

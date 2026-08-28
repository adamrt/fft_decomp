#include "psx/gs.h"
#include "psx/types.h"

void world_gs_gettiminfo(u32* tim, GsIMAGE* im) {
    u32* image;

    im->pmode = *tim;
    if ((im->pmode >> 3) & 1) {
        tim++;
        image = tim + (*tim >> 2);
        tim++;
        im->cx = ((u16*)tim)[0];
        im->cy = ((u16*)tim)[1];
        tim++;
        im->cw = ((u16*)tim)[0];
        im->ch = ((u16*)tim)[1];
        tim++;
        im->clut = tim;
        image++;
        im->px = ((u16*)image)[0];
        im->py = ((u16*)image)[1];
        image++;
        im->pw = ((u16*)image)[0];
        im->ph = ((u16*)image)[1];
        image++;
        im->pixel = image;
    } else {
        tim += 2;
        im->px = ((u16*)tim)[0];
        im->py = ((u16*)tim)[1];
        tim++;
        im->pw = ((u16*)tim)[0];
        im->ph = ((u16*)tim)[1];
        tim++;
        im->pixel = tim;
    }
}

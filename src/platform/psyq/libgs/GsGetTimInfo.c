#include "psx/libgs.h"
#include "psx/types.h"

/* Parsing through 32-bit values retains the retail parsing steps; direct RECT fields remove seven instructions. */
void GsGetTimInfo(u32* tim, GsIMAGE* im) {
    u32* image;

    im->pmode = *tim;
    /* The shifted palette test retains both native bit operations. */
    if ((im->pmode >> PSYQ_TIM_CLUT_SHIFT) & 1) {
        tim++;
        image = tim + (((psyq_tim_block_t*)tim)->size_bytes >> 2);
        tim = (u32*)&((psyq_tim_block_t*)tim)->rect;
        im->cx = *tim;
        im->cy = ((u16*)tim)[1];
        tim++;
        im->cw = *tim;
        im->ch = ((u16*)tim)[1];
        tim++;
        im->clut = tim;
        image = (u32*)&((psyq_tim_block_t*)image)->rect;
        im->px = *image;
        im->py = ((u16*)image)[1];
        image++;
        im->pw = *image;
        im->ph = ((u16*)image)[1];
        image++;
        im->pixel = image;
    } else {
        tim = (u32*)&((psyq_tim_block_t*)(tim + 1))->rect;
        im->px = *tim;
        im->py = ((u16*)tim)[1];
        tim++;
        im->pw = *tim;
        im->ph = ((u16*)tim)[1];
        tim++;
        im->pixel = tim;
    }
}

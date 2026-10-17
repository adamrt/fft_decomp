#include "psx/libetc.h"
/* Zero count callback slots. This source also builds psyq_etc_clear_dma_callbacks (0x8001e840),
 * the DMA object's identical copy. */
void psyq_etc_clear_vblank_callbacks(psyq_interrupt_callback_t* callbacks, s32 count) {
    s32 i;
    for (i = count - 1; i != -1; i--) {
        *callbacks++ = 0;
    }
}

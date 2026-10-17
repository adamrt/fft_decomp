#include "psx/libetc.h"
/* Zero count words. The intr, VSync and DMA objects each link an identical private copy
 * (0x8001e470, 0x8001e598, 0x8001e840). */
void psyq_etc_clear_interrupt_state_words(u32* words, s32 count) {
    s32 i;
    for (i = count - 1; i != -1; i--) {
        *words++ = 0;
    }
}

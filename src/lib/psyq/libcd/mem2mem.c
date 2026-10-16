/* SCUS_942.21 0x80021c5c..0x80021c8f. */
#include "psx/libcd.h"

/* CPU copy of words 32-bit words. StCdInterrupt uses it instead of dma_execute when the stream is
 * emulated from RAM (g_psyq_cd_stream_emulation_base set); the fourth argument, in dma_execute's
 * enable_irq position, is unused. */
void mem2mem(u32* destination, u32* source, u32 words, s32 unused_frame_complete) {
    u32 word_index;
    for (word_index = 0; word_index < words; word_index++)
        destination[word_index] = source[word_index];
}

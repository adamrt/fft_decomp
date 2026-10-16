/* SCUS_942.21 0x80021c90..0x80021e4b. */
#include "psx/libcd.h"
enum {
    CD_DMA_TRANSFER_ACTIVE = 0x01000000,
    CD_DMA_BUSY_POLL_LIMIT = 0x10000,
    CD_DMA_IRQ_ENABLE_REQUEST = 1,
    CD_DMA_IRQ_ENABLE_BYTE = 2,
    CD_DMA_PRIORITY_FIELD_BITS = 4,
    CD_DMA_PRIORITY_ENABLE_BIT = 3,
    CD_STATUS_DATA_READY = 0x40,
};

#define CD_DMA_CHANNELS ((volatile psyq_dma_channel_t*)0x1f801080)
/* Start a DMA on channel into destination (blocks of words): wait for the channel to be free, set
 * or clear its completion interrupt, enable it in the DMA priority register, wait for CD data to
 * be ready, then write control to start it. */
void dma_execute(s32 channel, u32* destination, u32 blocks, u32 words, u32 control, u8 enable_irq, s32 unused) {
    s32 polls = 0;
    /* Volatile retains the native stack stores of both register readbacks. */
    volatile u32 observed_register;
    volatile u32* dma;
    u8 enabled;
    u32 value; /* shift count, then the 32-bit priority value */
    u32 bit;
    u32 mask;
    while (CD_DMA_CHANNELS[channel].channel_control & CD_DMA_TRANSFER_ACTIVE) {
        if (polls == CD_DMA_BUSY_POLL_LIMIT) {
            printf(g_psyq_cd_dma_timeout_format, CD_DMA_CHANNELS[channel].channel_control);
            break;
        }
        polls++;
    }
    if (enable_irq == CD_DMA_IRQ_ENABLE_REQUEST) {
        bit = 1 << channel;
        enabled = g_psyq_cd_stream_dma_interrupt_register->bytes[CD_DMA_IRQ_ENABLE_BYTE];
        /* Retail schedules the shift ahead of the register read; without this tie it lands after it. */
        __asm__("" : "=r"(bit) : "0"(bit));
        enabled |= bit;
        g_psyq_cd_stream_dma_interrupt_register->bytes[CD_DMA_IRQ_ENABLE_BYTE] = enabled;
        observed_register = g_psyq_cd_stream_dma_interrupt_register->word;
    } else {
        enabled = g_psyq_cd_stream_dma_interrupt_register->bytes[CD_DMA_IRQ_ENABLE_BYTE];
        enabled &= ~(1 << channel);
        g_psyq_cd_stream_dma_interrupt_register->bytes[CD_DMA_IRQ_ENABLE_BYTE] = enabled;
        observed_register = g_psyq_cd_stream_dma_interrupt_register->word;
    }
    value = channel << 2;
    value += CD_DMA_PRIORITY_ENABLE_BIT;
    mask = 1;
    mask <<= value;
    /* Named register stores add a move and change the retail allocation. */
    dma = &CD_DMA_CHANNELS[channel].address;
    value = *g_psyq_cd_stream_dma_priority_register;
    value |= mask;
    *g_psyq_cd_stream_dma_priority_register = value;
    *dma++ = (u32)destination;
    *dma++ = (blocks << 16) | words;
    while (!(*g_psyq_cd_stream_index_register & CD_STATUS_DATA_READY)) { }
    *dma = control;
    observed_register = *dma;
}

#include "psx/libetc.h"

psyq_interrupt_callback_t DMACallback(s32 channel, psyq_interrupt_callback_t callback) {
    return g_psyq_etc_dispatch->dma_callback(channel, callback);
}

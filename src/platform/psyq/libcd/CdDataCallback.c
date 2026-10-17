#include "psx/libcd.h"
#include "psx/libetc.h"

psyq_interrupt_callback_t CdDataCallback(psyq_interrupt_callback_t callback) {
    return DMACallback(PSYQ_CD_DMA_CHANNEL, callback);
}

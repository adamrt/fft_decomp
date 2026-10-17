#include "psx/libetc.h"

psyq_interrupt_callback_t InterruptCallback(s32 channel, psyq_interrupt_callback_t callback) {
    return g_psyq_etc_dispatch->irq_callback(channel, callback);
}

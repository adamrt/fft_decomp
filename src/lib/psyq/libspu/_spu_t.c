/* SCUS_942.21 0x80018e44..0x800190d3. */
#include "psx/libspu.h"
#include "psx/stdarg.h"

/* One step of an SPU DMA transfer, chosen by operation: set the transfer address (a byte address),
 * select write or read mode, or start the DMA of a buffer whose byte size is rounded up to 64-byte
 * blocks. Returns -2 when the SPU does not acknowledge in time, else 0. */
s32 _spu_t(s32 operation, ...) {
    va_list arguments;
    u32 wait;
    u32 dma_control_word;
    u32 argument;
    u16 control;
    va_start(arguments, operation);
    switch (operation) {
    case PSYQ_SPU_DMA_SET_ADDRESS:
        argument = va_arg(arguments, u32);
        _spu_tsa = argument >> _spu_mem_mode_plus;
        _spu_RXX->rxx.transfer_address = _spu_tsa;
        break;
    case PSYQ_SPU_DMA_WRITE:
        g_psyq_spu_dma_direction = 0;
        wait = 0;
        while (_spu_RXX->rxx.transfer_address != _spu_tsa) {
            if (++wait > PSYQ_SPU_POLL_LIMIT) {
                va_end(arguments);
                return -2;
            }
        }
        control = _spu_RXX->rxx.control;
        control = (control & PSYQ_SPU_TRANSFER_MODE_CLEAR) | PSYQ_SPU_TRANSFER_MODE_DMA_WRITE;
        _spu_RXX->rxx.control = control;
        break;
    case PSYQ_SPU_DMA_READ:
        g_psyq_spu_dma_direction = 1;
        wait = 0;
        while (_spu_RXX->rxx.transfer_address != _spu_tsa) {
            if (++wait > PSYQ_SPU_POLL_LIMIT) {
                va_end(arguments);
                return -2;
            }
        }
        control = _spu_RXX->rxx.control;
        control |= PSYQ_SPU_TRANSFER_MODE_DMA_READ;
        _spu_RXX->rxx.control = control;
        break;
    case PSYQ_SPU_DMA_START_TRANSFER:
        control = g_psyq_spu_dma_direction == 1 ? PSYQ_SPU_TRANSFER_MODE_DMA_READ : PSYQ_SPU_TRANSFER_MODE_DMA_WRITE;
        wait = 0;
        while ((_spu_RXX->rxx.control & PSYQ_SPU_TRANSFER_MODE_MASK) != control) {
            if (++wait > PSYQ_SPU_POLL_LIMIT) {
                va_end(arguments);
                return -2;
            }
        }
        if (g_psyq_spu_dma_direction == 1) {
            _spu_FsetDelayW();
        } else {
            _spu_FsetDelayR();
        }
        /* Reading this pointer slot as a 32-bit value preserves the native transfer setup. */
        argument = va_arg(arguments, u32);
        g_psyq_spu_dma_buffer = (void*)argument;
        argument = va_arg(arguments, u32);
        g_psyq_spu_dma_blocks = (argument >> 6) + ((argument & 0x3f) ? 1 : 0);
        *g_psyq_spu_dma_address_register = (u32)g_psyq_spu_dma_buffer;
        *g_psyq_spu_dma_block_register = (g_psyq_spu_dma_blocks << 16) | 0x10;
        dma_control_word = 0x01000201;
        if (g_psyq_spu_dma_direction == 1) {
            dma_control_word = 0x01000200;
        }
        *g_psyq_spu_dma_control_register = dma_control_word;
        break;
    }
    va_end(arguments);
    return 0;
}

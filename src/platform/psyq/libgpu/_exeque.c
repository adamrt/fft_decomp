#include "psx/libetc.h"
#include "psx/libgpu.h"

/* LIBGPU, retail 0x8002675c-0x80026a58. DMA callback processes queued operations and completion. */
int _exeque(void) {
    if (*g_psyq_gpu_dma_chcr & PSYQ_DMA_BUSY)
        return 1;
    g_psyq_gpu_queue_interrupt_mask = SetIntrMask(0);
    while (_qin != _qout && !(*g_psyq_gpu_dma_chcr & PSYQ_DMA_BUSY)) {
        if (((_qout + 1) & PSYQ_GPU_QUEUE_INDEX_MASK) == _qin
            && !(*(volatile psyq_interrupt_callback_t*)&g_psyq_gpu_environment.draw_sync_callback))
            DMACallback(PSYQ_DMA_GPU_CHANNEL, 0);
        while (!(*g_psyq_gpu_gp1_port & PSYQ_GPU_STATUS_READY_COMMAND))
            ;
        _que[_qout].request.operation(_que[_qout].request.source, _que[_qout].request.argument);
        _qlog.operation = _que[_qout].request.operation;
        _qlog.source = _que[_qout].request.source;
        _qlog.argument = _que[_qout].request.argument;
        _qout = (_qout + 1) & PSYQ_GPU_QUEUE_INDEX_MASK;
    }
    SetIntrMask(g_psyq_gpu_queue_interrupt_mask);
    /* Ordinary field accesses move the completion store and omit the callback reload. */
    if (_qin == _qout && !(*g_psyq_gpu_dma_chcr & PSYQ_DMA_BUSY)
        && (*(volatile s32*)&g_psyq_gpu_environment.completion_pending)
        && (*(volatile psyq_interrupt_callback_t*)&g_psyq_gpu_environment.draw_sync_callback)) {
        (*(volatile s32*)&g_psyq_gpu_environment.completion_pending) = 0;
        (*(volatile psyq_interrupt_callback_t*)&g_psyq_gpu_environment.draw_sync_callback)();
    }
    return (_qin - _qout) & PSYQ_GPU_QUEUE_INDEX_MASK;
}

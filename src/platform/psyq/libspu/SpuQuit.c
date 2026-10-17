/* SCUS_942.21 0x80019448..0x800194c3. */
#include "psx/libspu.h"

#include "psx/libapi.h"

/* Shut the SPU library down once: clear the transfer, IRQ and DMA callbacks and
 * release the DMA event. The event is closed before it is disabled, so
 * DisableEvent acts on a closed descriptor; the order is the library's. */
void SpuQuit(void) {
    if (_spu_isCalled == 1) {
        _spu_isCalled = 0;
        EnterCriticalSection();
        _spu_transferCallback = 0;
        _spu_IRQCallback = 0;
        _SpuDataCallback(0);
        CloseEvent(_spu_EVdma);
        DisableEvent(_spu_EVdma);
        ExitCriticalSection();
    }
}

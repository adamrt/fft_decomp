/* SCUS_942.21 0x8001b04c..0x8001b06f. */
#include "psx/libspu.h"

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func) {
    SpuTransferCallbackProc previous = _spu_transferCallback;
    if (func != previous) {
        _spu_transferCallback = func;
    }
    return previous;
}

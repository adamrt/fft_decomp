#include "psx/libcd.h"

/* Default sync callback CdInit installs: deliver the HwCdRom EvSpCOMP event. */
void def_cbsync(u8 unused_event, u8* unused_result) {
    DeliverEvent(HwCdRom, EvSpCOMP);
}

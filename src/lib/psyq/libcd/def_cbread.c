#include "psx/libcd.h"

/* Default CdRead callback CdInit installs: delivers the same HwCdRom EvSpDR event as def_cbready. */
void def_cbread(u8 unused_event, u8* unused_result) {
    DeliverEvent(HwCdRom, EvSpDR);
}

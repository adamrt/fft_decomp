#include "psx/libcd.h"

/* Default ready callback CdInit installs: deliver the HwCdRom EvSpDR event. */
void def_cbready(u8 unused_event, u8* unused_result) {
    DeliverEvent(HwCdRom, EvSpDR);
}

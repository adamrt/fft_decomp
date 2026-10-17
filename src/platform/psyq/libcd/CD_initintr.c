/* SCUS_942.21 0x800202a8..0x800202f7. */
#include "psx/libcd.h"
#include "psx/libetc.h"

/* Clear the CD callbacks and status bytes and install callback on the CD-ROM interrupt (CdReset
 * mode 2). */
void CD_initintr(void) {
    CD_cbready = 0;
    CD_cbsync = 0;
    CD_status1 = 0;
    CD_status = 0;
    ResetCallback();
    InterruptCallback(2, callback);
}

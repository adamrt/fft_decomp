/* SCUS_942.21 0x80020760..0x8002083f. */
#include "psx/libcd.h"

/* CD-ROM IRQ handler: service each pending controller interrupt with getintr, call the ready and
 * sync callbacks, and restore the register bank index. */
void callback(void) {
    psyq_cd_dispatch_callbacks();
}

#include "psx/libgs.h"

/* Sets the base of the packet work area the sorters allocate their packets
 * from. */
void GsSetWorkBase(PACKET* base) {
    GsOUT_PACKET_P = base;
}

#include "psx/types.h"

/* The retail routine creates and immediately releases a 0x30-byte frame. */
void world_noop_800fad84(void) {
    volatile u8 unused[48];

    (void)unused;
}

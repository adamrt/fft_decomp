#include "psx/types.h"

/* Returns 0 with an 8-byte frame the retail build kept for an unreferenced
 * local. */
s32 battle_return_zero_801842f8(void) {
    char unused[8];

    return 0;
}

#include "psx/types.h"

/* The target allocates/releases a 48-byte stack frame, then returns.
 * The unused array reproduces the frame without claiming original locals. */
void battle_noop_80147918(void) {
    s32 unused[12];
}

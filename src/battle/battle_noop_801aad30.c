#include "psx/types.h"

/* The 12-byte target stores both arguments to their incoming stack home slots
 * and returns. Taking their addresses reproduces those stores; it does not
 * establish what the original source body contained. */
void battle_noop_801aad30(s32 ignored_1, s32 ignored_2) {
    s32* unused;
    unused = &ignored_1;
    unused = &ignored_2;
}

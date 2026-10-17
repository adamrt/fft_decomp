#include "psx/libcard.h"

/* Leaves the return value unspecified. */
s32 StopCARD(void) {
    StopCARD2();
    _patch_card2();
    psyq_card_restore_exception_prefix();
}

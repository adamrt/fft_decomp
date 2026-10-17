#include "psx/libcard.h"

void InitCARD(s32 mode) {
    ChangeClearPAD(0);
    EnterCriticalSection();
    InitCARD2(mode);
    _patch_card();
    _patch_card2();
    ExitCriticalSection();
}

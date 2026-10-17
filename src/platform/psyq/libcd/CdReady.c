#include "psx/libcd.h"

int CdReady(int mode, u8* result) {
    return CD_ready(mode, result);
}

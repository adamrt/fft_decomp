#include "psx/libcd.h"

int CdSync(int mode, u8* result) {
    return CD_sync(mode, result);
}

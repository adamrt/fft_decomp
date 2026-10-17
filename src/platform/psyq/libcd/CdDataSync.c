#include "psx/libcd.h"

int CdDataSync(int mode) {
    return CD_datasync(mode);
}

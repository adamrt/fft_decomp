#include "psx/libcd.h"

int CdGetSector(void* destination, int words) {
    return CD_getsector(destination, words) == 0;
}

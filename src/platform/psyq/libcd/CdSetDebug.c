#include "psx/libcd.h"

s32 CdSetDebug(s32 level) {
    s32 previous = CD_debug;
    CD_debug = level;
    return previous;
}

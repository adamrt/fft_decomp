#include "psx/gte.h"

void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

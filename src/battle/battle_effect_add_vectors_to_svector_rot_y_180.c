#include "psx/gte.h"
#include "psx/types.h"

/* Only the low halfword of each input component is read. */
/* result = left + right rotated 180 degrees about Y, i.e. (x - rx, y + ry, z - rz). */
void battle_effect_add_vectors_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = (u16)left->vx - (u16)right->vx;
    result->vy = (u16)left->vy + (u16)right->vy;
    result->vz = (u16)left->vz - (u16)right->vz;
}

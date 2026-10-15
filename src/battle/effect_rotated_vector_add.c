/* Vector add with 0/90/180/270 degree Y rotations. */

#include "fft/battle_effect.h"
#include "psx/libgte.h"

void effect_add_vectors_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vz;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 270 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x - rz, y + ry, z + rx). */
void effect_add_vectors_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vx;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 180 degrees about Y, i.e. (x - rx, y + ry, z - rz). */
void effect_add_vectors_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vz;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 90 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x + rz, y + ry, z - rx). */
void effect_add_vectors_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vx;
}

#include "fft/battle_effect.h"
#include "psx/libgte.h"

void effect_load_vector(const vec3s16_t* src, VECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

void effect_load_svector(const vec3s16_t* src, SVECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

void effect_shift_vector_right_12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx >> 12;
    dst->vy = src->vy >> 12;
    dst->vz = src->vz >> 12;
}

/* Convert integral vector components to 20.12 fixed point. */
void effect_scale_vector_fixed12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx * ONE;
    dst->vy = src->vy * ONE;
    dst->vz = src->vz * ONE;
}

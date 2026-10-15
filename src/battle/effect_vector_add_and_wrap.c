#include "fft/battle_effect.h"
#include "psx/libgte.h"

/* Wrap three angles into one turn (0 to 0xfff). */
void effect_mask_vector_low_12_bits(const SVECTOR* src, SVECTOR* dst) {
    dst->vx = src->vx & 0xFFF;
    dst->vy = src->vy & 0xFFF;
    dst->vz = src->vz & 0xFFF;
}

void effect_add_vectors(const VECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

void effect_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

void effect_add_svector_to_vector(const SVECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

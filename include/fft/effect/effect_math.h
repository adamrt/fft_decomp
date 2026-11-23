#ifndef EFFECT_MATH_H
#define EFFECT_MATH_H

/* Vector arithmetic and interpolation used by effects. */

#include "fft/gfx/gfx.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* vectors */
void effect_vector_load(const vec3s16_t* src, VECTOR* dst);
void effect_vector_load_svector(const vec3s16_t* src, SVECTOR* dst);
void effect_vector_add(const VECTOR* first, const VECTOR* second, VECTOR* out);
void effect_vector_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void effect_vector_add_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result);
void effect_vector_add_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out);
void effect_vector_add_rotations_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out);
void effect_vector_add_random_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void effect_vector_add_random_offsets_and_store_q12(const VECTOR* source, const VECTOR* ranges, VECTOR* destination);
void effect_vector_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination);
void effect_vector_scale_q12(const VECTOR* src, VECTOR* dst);
void effect_vector_shift_right_12(const VECTOR* src, VECTOR* dst);
void effect_vector_mask_low_12_bits(const SVECTOR* src, SVECTOR* dst);

/* magnitudes */
s16 effect_s16_select_largest_magnitude(s16 first, s16 second, s16 third);

/* interpolation */
s32 effect_s32_interpolate_cos(s32 start, s32 end, s32 duration, s32 current);
void effect_svector_interpolate_linear(const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_svector_interpolate_cos(const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_svector_interpolate_cos_out_back(
    const SVECTOR* from, const SVECTOR* to, s32 duration, s32 current, SVECTOR* out);
void effect_vector_interpolate_linear_q13(const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void effect_vector_interpolate_cos_q13(const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);
void effect_vector_interpolate_cos_out_back_q13(
    const VECTOR* from, const VECTOR* to, s32 duration, s32 current, VECTOR* out);

/* start + (end - start) * factor / 256, the division an arithmetic shift. */
s32 effect_lerp_q8(s32 start, s32 end, s32 factor);

#endif

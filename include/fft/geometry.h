#ifndef FFT_GEOMETRY_H
#define FFT_GEOMETRY_H

#include "psx/types.h"

/* Neutral 2D/3D point, rectangle and colour records for shapes the Psy-Q SDK
 * does not define.
 *
 * Use the SDK type whenever its layout and signedness fit exactly: DVECTOR
 * for GTE screen coordinates (SXY), SVECTOR/VECTOR for padded GTE vectors,
 * RECT for signed x/y/w/h rectangles and CVECTOR for r/g/b plus a code or
 * padding byte. These types cover the remaining cases. Field signedness is
 * part of the choice, not a detail: s16 fields load with lh and u16 fields
 * with lhu, so pick the variant whose loads match the target.
 *
 * Battle real positions are VECTORs (vx = x, vy = height, vz = map depth) and
 * screen positions are battle_screen_coords_t in X, Z, Y order; keep those in
 * their named types rather than vec3s16_t/vec3u16_t. */

/* Signed 16-bit x/y pair: screen offsets, sprite positions and per-axis
 * factors that are not GTE output. */
typedef struct point16 {
    s16 x;
    s16 y;
} point16_t;

/* Unsigned 16-bit x/y pair, e.g. VRAM or texture-page positions. */
typedef struct upoint16 {
    u16 x;
    u16 y;
} upoint16_t;

/* Unpadded signed 16-bit x/y/z triple (6 bytes; SVECTOR is 8). */
typedef struct vec3s16 {
    s16 x;
    s16 y;
    s16 z;
} vec3s16_t;

/* Unpadded unsigned 16-bit x/y/z triple (6 bytes). */
typedef struct vec3u16 {
    u16 x;
    u16 y;
    u16 z;
} vec3u16_t;

/* RECT layout with unsigned fields, for rectangles the target reads with lhu. */
typedef struct urect16 {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} urect16_t;

/* Three packed colour bytes with no fourth byte (CVECTOR is 4). */
typedef struct rgb8 {
    u8 r;
    u8 g;
    u8 b;
} rgb8_t;

typedef char assert_point16_size[sizeof(point16_t) == 4 ? 1 : -1];
typedef char assert_upoint16_size[sizeof(upoint16_t) == 4 ? 1 : -1];
typedef char assert_vec3s16_size[sizeof(vec3s16_t) == 6 ? 1 : -1];
typedef char assert_vec3u16_size[sizeof(vec3u16_t) == 6 ? 1 : -1];
typedef char assert_urect16_size[sizeof(urect16_t) == 8 ? 1 : -1];
typedef char assert_rgb8_size[sizeof(rgb8_t) == 3 ? 1 : -1];

#endif

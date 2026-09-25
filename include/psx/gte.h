#ifndef FFT_PSX_GTE_H
#define FFT_PSX_GTE_H

#include "psx/types.h"

#define ONE 4096 /* 1.0 in GTE fixed point (1.3.12 / 20.12); one full turn for rsin/rcos */

typedef struct {
    s16 vx;
    s16 vy;
    s16 vz;
    s16 pad;
} SVECTOR;

typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* Psy-Q 2D short vector, e.g. a GTE screen coordinate (SXY). */
typedef struct {
    s16 vx;
    s16 vy;
} DVECTOR;

/* Psy-Q colour vector: r, g, b plus the GPU code byte. */
typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 cd;
} CVECTOR;

typedef char assert_matrix_size[sizeof(MATRIX) == 32 ? 1 : -1];
typedef char assert_svector_size[sizeof(SVECTOR) == 8 ? 1 : -1];
typedef char assert_vector_size[sizeof(VECTOR) == 16 ? 1 : -1];
typedef char assert_dvector_size[sizeof(DVECTOR) == 4 ? 1 : -1];
typedef char assert_cvector_size[sizeof(CVECTOR) == 4 ? 1 : -1];

extern void SetGeomOffset(int, int);
extern void SetGeomScreen(int);
extern void InitGeom(void);
extern void SetRotMatrix(MATRIX* m);
extern void SetTransMatrix(MATRIX* m);
extern s32 rsin(s32 angle);
extern s32 rcos(s32 angle);
extern long ratan2(long y, long x);
extern long RotTrans(SVECTOR* input, VECTOR* output, long* flag);
extern long csqrt(long value);
extern long SquareRoot0(long value);
extern void RotMatrix(SVECTOR* r, MATRIX* m);

/* Unnamed libgte routines (0x8001c068-0x8001d5d8); declarations follow
 * their game call sites until the Psy-Q names are verified. */
void VectorNormal(VECTOR* input, VECTOR* output);
s32 SquareRoot12(s32 value);
void MulMatrix0(MATRIX* m0, void* m1, MATRIX* m2);
void ApplyMatrixLV(MATRIX* m, VECTOR* in, VECTOR* out);
void ScaleMatrixL(MATRIX* matrix, VECTOR* scale);
void PushMatrix(void);
void PopMatrix(void);
void MulMatrix(MATRIX* matrix, MATRIX* other);
void MulMatrix2(MATRIX* m0, MATRIX* m1);
void TransMatrix(void* camera_matrix, void* offset_screen_coords);
void ScaleMatrix(void* camera_matrix, void* scale);
void SetLightMatrix(MATRIX* m);
void SetColorMatrix(MATRIX* m);
void SetBackColor(s32, s32, s32);
void SetFarColor(s32, s32, s32);
/* RotTransPers-like by argument shape. */
void RotTransSV(SVECTOR* in, SVECTOR* out, s32* flag);
s32 RotTransPers(SVECTOR* v0, s32* sxy0, s32* p, s32* flag);
s32 RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, s32* sxy0, s32* sxy1, s32* sxy2, s32* sxy3, s32* p, s32* flag);
s32 ReadGeomScreen(void);
s32 RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, s32* sxy0, s32* sxy1, s32* sxy2, s32* p, s32* flag);
/* Psy-Q NormalClip: the sign of the screen-space winding of three packed XY points. */
long NormalClip(long sxy0, long sxy1, long sxy2);

#endif

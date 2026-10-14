#ifndef PSX_LIBGTE_H
#define PSX_LIBGTE_H

#include "psx/gte_regs.h"
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

/* Psy-Q color vector: r, g, b plus the GPU code byte. */
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

/* geometry setup */
extern void InitGeom(void);
s32 ReadGeomScreen(void);
extern void SetGeomOffset(int, int);
extern void SetGeomScreen(int);

/* matrix state */
void PopMatrix(void);
void PushMatrix(void);
void ReadRotMatrix(MATRIX* output);
extern void SetRotMatrix(MATRIX* m);
extern void SetTransMatrix(MATRIX* m);

/* matrix operations */
MATRIX* CompMatrix(MATRIX* left, MATRIX* right, MATRIX* output);
void MatrixNormal(MATRIX* input, MATRIX* output);
MATRIX* MulMatrix(MATRIX* matrix, MATRIX* other);
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2);
MATRIX* MulMatrix2(MATRIX* m0, MATRIX* m1);
MATRIX* MulRotMatrix(MATRIX* matrix);
MATRIX* MulRotMatrix0(MATRIX* matrix, MATRIX* output);
extern MATRIX* RotMatrix(SVECTOR* r, MATRIX* m);
MATRIX* ScaleMatrix(MATRIX* camera_matrix, VECTOR* scale);
MATRIX* ScaleMatrixL(MATRIX* matrix, VECTOR* scale);
MATRIX* SetMulMatrix(MATRIX* left, MATRIX* right);
MATRIX* TransMatrix(MATRIX* camera_matrix, VECTOR* offset_screen_coords);

/* projection */
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* in, VECTOR* out);
/* Sign of the screen-space winding of three packed XY points. */
long NormalClip(long sxy0, long sxy1, long sxy2);
extern void RotTrans(SVECTOR* input, VECTOR* output, long* flag);
s32 RotTransPers(SVECTOR* v0, s32* sxy0, s32* p, s32* flag);
s32 RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, s32* sxy0, s32* sxy1, s32* sxy2, s32* p, s32* flag);
s32 RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, s32* sxy0, s32* sxy1, s32* sxy2, s32* sxy3, s32* p, s32* flag);
/* Transform with signed 16-bit vector output. */
void RotTransSV(SVECTOR* in, SVECTOR* out, s32* flag);

/* lighting */
void DpqColor3(CVECTOR* c0, CVECTOR* c1, CVECTOR* c2, s32 depth, CVECTOR* out0, CVECTOR* out1, CVECTOR* out2);
void DpqColorLight(VECTOR* input, CVECTOR* color, s32 depth, CVECTOR* output);
void Intpl(VECTOR* input, s32 depth, CVECTOR* output);
void LightColor(VECTOR* input, VECTOR* output);
void ReadColorMatrix(MATRIX* output);
void ReadLightMatrix(MATRIX* output);
void SetBackColor(s32, s32, s32);
void SetColorMatrix(MATRIX* m);
void SetFarColor(s32, s32, s32);
void SetLightMatrix(MATRIX* m);

/* vectors */
void OuterProduct0(VECTOR* a, VECTOR* b, VECTOR* output);
void OuterProduct12(VECTOR* a, VECTOR* b, VECTOR* output);
VECTOR* Square0(VECTOR* input, VECTOR* output);
VECTOR* Square12(VECTOR* input, VECTOR* output);
VECTOR* SquareSL0(SVECTOR* input, VECTOR* output);
VECTOR* SquareSL12(SVECTOR* input, VECTOR* output);
SVECTOR* SquareSS0(SVECTOR* input, SVECTOR* output);
SVECTOR* SquareSS12(SVECTOR* input, SVECTOR* output);
long VectorNormal(VECTOR* input, VECTOR* output);
long VectorNormalS(VECTOR* input, SVECTOR* output);
long VectorNormalSS(SVECTOR* input, SVECTOR* output);

/* scalar math */
s32 AverageZ3(s32 z0, s32 z1, s32 z2);
s32 AverageZ4(s32 z0, s32 z1, s32 z2, s32 z3);
void InvSquareRoot(long value, long* mantissa_out, long* exponent_out);
s32 Lzc(s32 value);
extern long SquareRoot0(long value);
s32 SquareRoot12(s32 value);
extern long csqrt(long value);
extern long ratan2(long y, long x);
extern s32 rcos(s32 angle);
extern s32 rsin(s32 angle);

/* driver internals */

/* state */
extern u32 g_psyq_gte_init_saved_ra;
extern u32 g_psyq_gte_matrix_stack_saved_ra;
extern s32 g_psyq_gte_matrix_stack_offset;
extern MATRIX g_psyq_gte_matrix_stack[20];
extern u32 g_psyq_gte_patch_saved_ra;
extern const char g_psyq_gte_matrix_stack_overflow_message[];
extern const char g_psyq_gte_matrix_stack_underflow_message[];
extern u32 g_psyq_gte_exception_patch_template[];
extern u32 g_psyq_gte_exception_patch_template_end[];

/* Tables stay in their original data intervals. */
extern s16 g_psyq_gte_sine_quarter_table[];
extern s16 g_psyq_gte_atan_ratio_table[];
extern s16 g_psyq_gte_sqrt_table[];
extern s16 g_psyq_gte_inv_sqrt_table[];
extern u32 g_psyq_gte_sin_cos_table[];

void _patch_gte(void);
u32 psyq_gte_apply_rotation_ir(SVECTOR* input, VECTOR* output, u32 original_a2);
s32 psyq_gte_csqrt_kernel(s32 value);
/* Private handwritten ABI: t0-t2 are input/output; v0 is the squared length. */
void psyq_gte_normalize_register_vector(void);
s32 sin_1(s32 angle);

#endif

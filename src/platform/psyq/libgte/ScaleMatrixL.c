#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001c9e0–0x8001cb04: row scaling, the low 32 bits of products and 32-bit stores.
 * 32-bit accesses preserve writes to MATRIX's 16-bit alignment field. */
MATRIX* ScaleMatrixL(MATRIX* matrix, VECTOR* scale) {
    /* Fixed scratch registers preserve the handwritten multiply/store sequence. */
    register u32 packed __asm__("$8");
    register s32 low __asm__("$9");
    register s32 high __asm__("$10");
    register u32 scale_x __asm__("$11");
    register u32 scale_y __asm__("$12");
    register u32 scale_z __asm__("$13");
    MATRIX* result;
    packed = *(u32*)&matrix->m[0][0];
    scale_x = scale->vx;
    low = packed & 0xffff;
    __asm__("" : "=r"(low) : "0"(low)); /* Keep the retail redundant low-half mask. */
    low = (low << 16) >> 16;
    PSYQ_GTE_MULTU_BEGIN(low, scale_x);
    high = (s32)packed >> 16;
    scale_y = scale->vy;
    scale_z = scale->vz;
    /* Both scale loads precede the next 32-bit matrix value. */
    __asm__ volatile("" : : "r"(scale_y), "r"(scale_z) : "memory");
    packed = *(u32*)&matrix->m[0][2];
    /* This 32-bit value loads before the result-pointer copy. */
    __asm__ volatile("" : "=r"(matrix) : "0"(matrix), "r"(packed) : "memory");
    __asm__("" : "=r"(result) : "0"(matrix)); /* Retail copies the result before the packed stores. */
    PSYQ_GTE_READ_LO(low);
    low = (low >> 12) & 0xffff;
    PSYQ_GTE_MULTU_BEGIN(high, scale_x);
    PSYQ_GTE_READ_LO(high);
    high = (high >> 12) << 16;
    low |= high;
    *(u32*)&matrix->m[0][0] = low;
    low = packed & 0xffff;
    __asm__("" : "=r"(low) : "0"(low));
    low = (low << 16) >> 16;
    PSYQ_GTE_MULTU_BEGIN(low, scale_x);
    high = (s32)packed >> 16;
    packed = *(u32*)&matrix->m[1][1];
    PSYQ_GTE_READ_LO(low);
    low = (low >> 12) & 0xffff;
    PSYQ_GTE_MULTU_BEGIN(high, scale_y);
    PSYQ_GTE_READ_LO(high);
    high = (high >> 12) << 16;
    low |= high;
    *(u32*)&matrix->m[0][2] = low;
    low = packed & 0xffff;
    __asm__("" : "=r"(low) : "0"(low));
    low = (low << 16) >> 16;
    PSYQ_GTE_MULTU_BEGIN(low, scale_y);
    high = (s32)packed >> 16;
    packed = *(u32*)&matrix->m[2][0];
    PSYQ_GTE_READ_LO(low);
    low = (low >> 12) & 0xffff;
    PSYQ_GTE_MULTU_BEGIN(high, scale_y);
    PSYQ_GTE_READ_LO(high);
    high = (high >> 12) << 16;
    low |= high;
    *(u32*)&matrix->m[1][1] = low;
    low = packed & 0xffff;
    __asm__("" : "=r"(low) : "0"(low));
    low = (low << 16) >> 16;
    PSYQ_GTE_MULTU_BEGIN(low, scale_z);
    high = (s32)packed >> 16;
    packed = *(u32*)&matrix->m[2][2];
    PSYQ_GTE_READ_LO(low);
    low = (low >> 12) & 0xffff;
    PSYQ_GTE_MULTU_BEGIN(high, scale_z);
    PSYQ_GTE_READ_LO(high);
    high = (high >> 12) << 16;
    low |= high;
    *(u32*)&matrix->m[2][0] = low;
    low = packed & 0xffff;
    __asm__("" : "=r"(low) : "0"(low));
    low = (low << 16) >> 16;
    PSYQ_GTE_MULTU_BEGIN(low, scale_z);
    PSYQ_GTE_READ_LO(low);
    low >>= 12;
    *(u32*)&matrix->m[2][2] = low;
    return result;
}

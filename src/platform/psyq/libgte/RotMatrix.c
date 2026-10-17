#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001d658–0x8001d8e4: construct rotation from the packed sin/cos table.
 * The low 32 bits of unsigned products and negation before rescaling retain rounding. */
MATRIX* RotMatrix(SVECTOR* rotation, MATRIX* output) {
    MATRIX* result;
    register s32 cosine_x __asm__("$8");
    register s32 cosine_y __asm__("$9");
    register s32 cosine_z __asm__("$10");
    register s32 sine_x __asm__("$11");
    register s32 negative_sine_y __asm__("$12");
    register s32 sine_z __asm__("$13");
    register s32 value __asm__("$14");
    register s32 angle __asm__("$15");
    register s32 temporary __asm__("$24");
    register s32 packed __asm__("$25");
    angle = rotation->vx;
    result = output;
    packed = angle & 0xfff;
    if (angle >= 0)
        goto positive_x;
    angle = 0U - angle;
    PSYQ_GTE_MASK_BRANCH_BEGIN(angle);
    angle &= 0xfff;
    PSYQ_GTE_MASK_BRANCH_END(angle);
    temporary = angle << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    temporary = packed << 16;
    temporary >>= 16;
    sine_x = 0U - temporary;
    cosine_x = packed >> 16;
    goto x_ready;
positive_x:
    temporary = packed << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    temporary = packed << 16;
    sine_x = temporary >> 16;
    cosine_x = packed >> 16;
    __asm__(""
        : "=r"(cosine_x)
        : "0"(cosine_x)); /* Keep the signed-angle arms separate; negative-arm cosine fills its jump delay. */
x_ready:
    angle = rotation->vy;
    packed = angle & 0xfff;
    if (angle >= 0)
        goto positive_y;
    angle = 0U - angle;
    PSYQ_GTE_MASK_BRANCH_BEGIN(angle);
    angle &= 0xfff;
    PSYQ_GTE_MASK_BRANCH_END(angle);
    temporary = angle << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    negative_sine_y = packed << 16;
    negative_sine_y >>= 16;
    value = 0U - negative_sine_y;
    cosine_y = packed >> 16;
    goto y_ready;
positive_y:
    temporary = packed << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    value = packed << 16;
    value >>= 16;
    negative_sine_y = 0U - value;
    cosine_y = packed >> 16;
    __asm__(""
        : "=r"(cosine_y)
        : "0"(cosine_y)); /* Keep the signed-angle arms separate; negative-arm cosine fills its jump delay. */
y_ready:
    PSYQ_GTE_MULTU_BEGIN(cosine_y, sine_x);
    angle = rotation->vz;
    output->m[0][2] = value;
    PSYQ_GTE_READ_LO(temporary);
    packed = 0U - temporary;
    value = packed >> 12;
    PSYQ_GTE_MULTU_BEGIN(cosine_y, cosine_x);
    output->m[1][2] = value;
    packed = angle & 0xfff;
    if (angle >= 0)
        goto positive_z;
    PSYQ_GTE_READ_LO(temporary);
    value = temporary >> 12;
    output->m[2][2] = value;
    __asm__ volatile(""
        : "=r"(angle)
        : "0"(angle)
        : "memory"); /* Finish the output store before negating the next angle. */
    angle = 0U - angle;
    PSYQ_GTE_MASK_BRANCH_BEGIN(angle);
    angle &= 0xfff;
    PSYQ_GTE_MASK_BRANCH_END(angle);
    temporary = angle << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    temporary = packed << 16;
    temporary >>= 16;
    sine_z = 0U - temporary;
    cosine_z = packed >> 16;
    goto z_ready;
positive_z:
    PSYQ_GTE_READ_LO(angle);
    value = angle >> 12;
    output->m[2][2] = value;
    __asm__ volatile("" : : : "memory"); /* Finish the store before the paired table relocation sequence. */
    temporary = packed << 2;
    PSYQ_GTE_TABLE_HIGH_PAIRED(packed, g_psyq_gte_sin_cos_table);
    packed += temporary;
    __asm__(""
        : "=r"(packed)
        : "0"(packed)); /* Preserve packed 32-bit fetch/extraction; removing this contracts to LH. */
    packed = *(u32*)packed;
    temporary = packed << 16;
    sine_z = temporary >> 16;
    cosine_z = packed >> 16;
    __asm__(""
        : "=r"(cosine_z)
        : "0"(cosine_z)); /* Keep the signed-angle arms separate; negative-arm cosine fills its jump delay. */
z_ready:
    PSYQ_GTE_MULTU_BEGIN(cosine_z, cosine_y);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    value = angle >> 12;
    output->m[0][0] = value;
    PSYQ_GTE_MULTU_BEGIN(sine_z, cosine_y);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    value = 0U - angle;
    angle = value >> 12;
    PSYQ_GTE_MULTU_BEGIN(cosine_z, negative_sine_y);
    output->m[0][1] = angle;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_READ_LO(angle);
    temporary = angle >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(temporary, sine_x);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    value = angle >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(sine_z, cosine_x);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    packed = angle >> 12;
    angle = packed - value;
    PSYQ_GTE_MULTU_BEGIN(temporary, cosine_x);
    output->m[1][0] = angle;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_READ_LO(value);
    angle = value >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(sine_z, sine_x);
    PSYQ_GTE_READ_LO_WAIT2(value);
    packed = value >> 12;
    value = packed + angle;
    PSYQ_GTE_MULTU_BEGIN(sine_z, negative_sine_y);
    output->m[2][0] = value;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_READ_LO(angle);
    temporary = angle >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(temporary, sine_x);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    value = angle >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(cosine_z, cosine_x);
    PSYQ_GTE_READ_LO_WAIT2(angle);
    packed = angle >> 12;
    angle = packed + value;
    PSYQ_GTE_MULTU_BEGIN(temporary, cosine_x);
    output->m[1][1] = angle;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_READ_LO(value);
    angle = value >> 12;
    PSYQ_GTE_MULTIPLY_WAIT();
    PSYQ_GTE_MULTU_BEGIN(cosine_z, sine_x);
    PSYQ_GTE_READ_LO_WAIT2(value);
    packed = value >> 12;
    value = packed - angle;
    output->m[2][1] = value;
    __asm__ volatile("" : : : "memory"); /* Keep the final empty return delay slot. */
    return result;
}

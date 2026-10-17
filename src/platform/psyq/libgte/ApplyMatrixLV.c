#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001c850–0x8001c9b0: two-pass signed wide-vector rotation.
 * Negative high parts are negated before shifting and restored afterward. */
VECTOR* ApplyMatrixLV(MATRIX* matrix, VECTOR* input, VECTOR* output) {
    MATRIX* rotation = matrix;
    VECTOR* vector = input;
    /* Fixed scratch registers preserve the decomposed vector transform. */
    register VECTOR* destination __asm__("$6") = output;
    register s32 x __asm__("$8");
    register s32 y __asm__("$9");
    register s32 z __asm__("$10");
    register s32 high_x __asm__("$11");
    register s32 high_y __asm__("$12");
    register s32 high_z __asm__("$13");
    register s32 zero __asm__("$0"); /* The hardware zero register preserves the retail relative joins. */
    PSYQ_GTE_LOAD_MATRIX(rotation, PSYQ_GTE_CTRL_R11_R12, PSYQ_GTE_CTRL_R13_R21, PSYQ_GTE_CTRL_R22_R23,
        PSYQ_GTE_CTRL_R31_R32, PSYQ_GTE_CTRL_R33);
    x = vector->vx;
    y = vector->vy;
    z = vector->vz;
    if (x < 0) {
        x = -x;
        high_x = x >> 15;
        x &= 0x7fff;
        high_x = -high_x;
        x = -x;
        if (zero == 0)
            goto x_ready;
    }
    high_x = x >> 15;
    x &= 0x7fff;
x_ready:
    if (y < 0) {
        y = -y;
        high_y = y >> 15;
        y &= 0x7fff;
        high_y = -high_y;
        y = -y;
        if (zero == 0)
            goto y_ready;
    }
    high_y = y >> 15;
    y &= 0x7fff;
y_ready:
    if (z < 0) {
        z = -z;
        high_z = z >> 15;
        z &= 0x7fff;
        high_z = -high_z;
        z = -z;
        if (zero == 0)
            goto z_ready;
    }
    high_z = z >> 15;
    z &= 0x7fff;
z_ready:
    PSYQ_GTE_APPLY_LONG_HIGH(high_x, high_y, high_z);
    PSYQ_GTE_APPLY_LONG_LOW(x, y, z);
    if (high_x < 0) {
        high_x = -high_x;
        high_x <<= 3;
        __asm__("" : "=r"(high_x) : "0"(high_x));
        high_x = -high_x;
        if (zero == 0)
            goto high_x_ready;
    }
    high_x <<= 3;
high_x_ready:
    if (high_y < 0) {
        high_y = -high_y;
        high_y <<= 3;
        __asm__("" : "=r"(high_y) : "0"(high_y));
        high_y = -high_y;
        if (zero == 0)
            goto high_y_ready;
    }
    high_y <<= 3;
high_y_ready:
    if (high_z < 0) {
        high_z = -high_z;
        high_z <<= 3;
        __asm__("" : "=r"(high_z) : "0"(high_z));
        high_z = -high_z;
        if (zero == 0)
            goto high_z_ready;
    }
    high_z <<= 3;
high_z_ready:
    PSYQ_GTE_READ_MAC(x, y, z);
    x += high_x;
    y += high_y;
    z += high_z;
    destination->vx = x;
    destination->vy = y;
    destination->vz = z;
    __asm__(""
        : "=r"(destination)
        : "0"(destination)
        : "memory"); /* Keep the retail return copy after all output stores. */
    return destination;
}

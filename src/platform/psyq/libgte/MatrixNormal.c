#include "psx/gte_inline.h"
#include "psx/libgte.h"

/* main 0x8001c180–0x8001c264: build normalized basis from the first two rows.
 * Input row2 is ignored. Only the rotation diagonal is restored. */
void MatrixNormal(MATRIX* input, MATRIX* output) {
    /* The GTE transport and private worker fix these scratch-register roles. */
    s32 x;
    s32 y;
    s32 z;
    register s32 second_x __asm__("$11");
    register s32 second_y __asm__("$12");
    register s32 second_z __asm__("$13");
    register s32 third_x __asm__("$15");
    register s32 third_y __asm__("$24");
    register s32 third_z __asm__("$25");
    s32 diagonal0;
    register s32 diagonal1 __asm__("$3");
    register s32 diagonal2 __asm__("$6");
    s32 sum;
    register u32 saved_ra __asm__("$7");
    x = input->m[0][0];
    y = input->m[0][1];
    z = input->m[0][2];
    second_x = input->m[1][0];
    second_y = input->m[1][1];
    second_z = input->m[1][2];
    PSYQ_GTE_READ_ROT_DIAGONAL(diagonal0, diagonal1, diagonal2);
    PSYQ_GTE_NORMAL_FIRST_CROSS(x, y, z, second_x, second_y, second_z, third_x, third_y, third_z);
    PSYQ_GTE_NORMAL_SECOND_CROSS(x, y, z, second_x, second_y, second_z);
    PSYQ_GTE_RESTORE_ROT_DIAGONAL(diagonal0, diagonal1, diagonal2);
    PSYQ_GTE_SAVE_RA(saved_ra);
    PSYQ_GTE_NORMALIZE_CALL(psyq_gte_normalize_register_vector, x, y, z, sum);
    output->m[0][0] = x;
    output->m[0][1] = y;
    output->m[0][2] = z;
    PSYQ_GTE_NORMAL_READ_SECOND(x, y, z);
    PSYQ_GTE_NORMALIZE_CALL(psyq_gte_normalize_register_vector, x, y, z, sum);
    output->m[1][0] = x;
    output->m[1][1] = y;
    output->m[1][2] = z;
    __asm__ volatile(""
        :
        :
        : "memory"); /* Complete second-row stores before the next scratch-register argument copies. */
    x = third_x;
    y = third_y;
    PSYQ_GTE_NORMALIZE_CALL_Z(psyq_gte_normalize_register_vector, x, y, z, sum, third_z);
    PSYQ_GTE_RESTORE_RA(saved_ra);
    output->m[2][0] = x;
    output->m[2][1] = y;
    output->m[2][2] = z;
}

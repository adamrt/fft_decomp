#include "psx/types.h"

/* Advance to the requested entry in a packed text section. */
const u8* world_text_skip_to_entry(const u8* text, s32 entry_index) {
    const u8* result;
    const u8* cursor = text;
    s32 index = entry_index;

    /* Hand-assembled in the retail binary: trapping addi/add, $t0-$t2
     * scratch, and unfilled delay slots (`b` with nop). No C form emits these. */
    __asm__ volatile("addi $9,$0,0\n"
                     "addi $10,$0,0xfe\n"
                     "1:\n"
                     "lbu $8,0($4)\n"
                     "beq $5,$9,2f\n"
                     "andi $8,$8,0xfe\n"
                     "bne $8,$10,3f\n"
                     "addi $9,$9,1\n"
                     "3:\n"
                     "addi $4,$4,1\n"
                     "bgez $0,1b\n"
                     "2:\n"
                     "add $2,$0,$4"
        : "=r"(result), "=r"(cursor)
        : "1"(cursor), "r"(index)
        : "$8", "$9", "$10");
    return result;
}

/* Skipping forward through text to a numbered entry. */

#include "psx/types.h"

/* Return a pointer to entry entry_index of a packed text section: entries follow one another,
 * each ended by a 0xFE or 0xFF byte, and the scan counts those terminators from text.
 *
 * The retail helper uses trapping arithmetic and fixed $t0-$t2 scratch
 * registers. The inline sequence preserves those instruction choices and the
 * deliberately empty branch delay slots. */
const u8* text_skip_to_entry(const u8* text, s32 entry_index) {
    const u8* result;
    const u8* cursor = text;
    s32 index = entry_index;

    __asm__("addi $9,$0,0\n"
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

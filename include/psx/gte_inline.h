#ifndef FFT_PSX_GTE_INLINE_H
#define FFT_PSX_GTE_INLINE_H

/* Clean-room GTE (COP2) operation macros.
 *
 * The retail game code issues GTE operations through the SDK's inline macros
 * in their DMPSX form, which C cannot express: every memory operand is first
 * copied to $12, and each GTE command is preceded by two nops. These macros
 * reproduce the instruction sequences observed in the target (for example the
 * EFFECT mesh renderers' ldv0/rtv0tr/stlvnl/stflg sequence) and keep the SDK's
 * names. Add an operation only with target evidence of its exact form. */

/* Load V0 from an SVECTOR. */
#define gte_ldv0(r0)                                                                                                   \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "lwc2 $0, 0($12);"                                                                                \
                     "lwc2 $1, 4($12)"                                                                                 \
        :                                                                                                              \
        : "r"(r0)                                                                                                      \
        : "$12")

/* Load V0, V1 and V2 from three SVECTORs. */
#define gte_ldv3(r0, r1, r2)                                                                                           \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "lwc2 $0, 0($12);"                                                                                \
                     "lwc2 $1, 4($12);"                                                                                \
                     "addu $12, %1, $0;"                                                                               \
                     "lwc2 $2, 0($12);"                                                                                \
                     "lwc2 $3, 4($12);"                                                                                \
                     "addu $12, %2, $0;"                                                                               \
                     "lwc2 $4, 0($12);"                                                                                \
                     "lwc2 $5, 4($12)"                                                                                 \
        :                                                                                                              \
        : "r"(r0), "r"(r1), "r"(r2)                                                                                    \
        : "$12")

/* MVMVA: rotate V0 by the rotation matrix and add the translation vector. */
#define gte_rtv0tr()                                                                                                   \
    __asm__ volatile("nop;"                                                                                            \
                     "nop;"                                                                                            \
                     ".word 0x4a480012")

/* RTPS: perspective-transform V0. */
#define gte_rtps()                                                                                                     \
    __asm__ volatile("nop;"                                                                                            \
                     "nop;"                                                                                            \
                     ".word 0x4a180001")

/* RTPT: perspective-transform V0, V1 and V2. */
#define gte_rtpt()                                                                                                     \
    __asm__ volatile("nop;"                                                                                            \
                     "nop;"                                                                                            \
                     ".word 0x4a280030")

/* Store MAC1-MAC3 to a VECTOR's three words. */
#define gte_stlvnl(r0)                                                                                                 \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "swc2 $25, 0($12);"                                                                               \
                     "swc2 $26, 4($12);"                                                                               \
                     "swc2 $27, 8($12)"                                                                                \
        :                                                                                                              \
        : "r"(r0)                                                                                                      \
        : "$12", "memory")

/* Store the FLAG control register to a word. */
#define gte_stflg(r0)                                                                                                  \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "cfc2 $13, $31;"                                                                                  \
                     "nop;"                                                                                            \
                     "sw $13, 0($12)"                                                                                  \
        :                                                                                                              \
        : "r"(r0)                                                                                                      \
        : "$12", "$13", "memory")

/* Store SXY2 (the last projected screen coordinate). */
#define gte_stsxy(r0)                                                                                                  \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "swc2 $14, 0($12)"                                                                                \
        :                                                                                                              \
        : "r"(r0)                                                                                                      \
        : "$12", "memory")

/* Store SXY0, SXY1 and SXY2. */
#define gte_stsxy3(r0, r1, r2)                                                                                         \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "swc2 $12, 0($12);"                                                                               \
                     "addu $12, %1, $0;"                                                                               \
                     "swc2 $13, 0($12);"                                                                               \
                     "addu $12, %2, $0;"                                                                               \
                     "swc2 $14, 0($12)"                                                                                \
        :                                                                                                              \
        : "r"(r0), "r"(r1), "r"(r2)                                                                                    \
        : "$12", "memory")

/* RotTrans: V0 = *r1, then store the rotated and translated MAC1-MAC3 to r2
 * and FLAG to r3. The empty clobber of $14/$15 reproduces the retail register
 * allocation around the sequence. */
#define gte_RotTrans(r1, r2, r3)                                                                                       \
    {                                                                                                                  \
        gte_ldv0(r1);                                                                                                  \
        gte_rtv0tr();                                                                                                  \
        gte_stlvnl(r2);                                                                                                \
        gte_stflg(r3);                                                                                                 \
        __asm__ volatile("" : : : "$14", "$15");                                                                       \
    }

/* The same RotTrans issued as one statement per instruction. Its larger RTL
 * insn count changes GCC's loop-invariant and strength-reduction decisions;
 * the 5,368-byte EFFECT ring renderers need this form. */
#define gte_RotTrans_split(r1, r2, r3)                                                                                 \
    {                                                                                                                  \
        __asm__ volatile("addu $12, %0, $0" : : "r"(r1) : "$12");                                                      \
        __asm__ volatile("lwc2 $0, 0($12)");                                                                           \
        __asm__ volatile("lwc2 $1, 4($12)");                                                                           \
        gte_rtv0tr();                                                                                                  \
        __asm__ volatile("addu $12, %0, $0" : : "r"(r2) : "$12");                                                      \
        __asm__ volatile("swc2 $25, 0($12)" : : : "memory");                                                           \
        __asm__ volatile("swc2 $26, 4($12)" : : : "memory");                                                           \
        __asm__ volatile("swc2 $27, 8($12)" : : : "memory");                                                           \
        __asm__ volatile("addu $12, %0, $0" : : "r"(r3) : "$12");                                                      \
        __asm__ volatile("cfc2 $13, $31" : : : "$13");                                                                 \
        __asm__ volatile("nop");                                                                                       \
        __asm__ volatile("sw $13, 0($12)" : : : "memory");                                                             \
        __asm__ volatile("" : : : "$14", "$15");                                                                       \
    }

/* Load the rotation matrix (control registers 0-4) from a MATRIX. The
 * "memory" clobber makes GCC reload pointers read after the macro. */
#define gte_SetRotMatrix(r)                                                                                            \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "lw $13, 0($12);"                                                                                 \
                     "lw $14, 4($12);"                                                                                 \
                     "ctc2 $13, $0;"                                                                                   \
                     "ctc2 $14, $1;"                                                                                   \
                     "lw $13, 8($12);"                                                                                 \
                     "lw $14, 12($12);"                                                                                \
                     "lw $15, 16($12);"                                                                                \
                     "ctc2 $13, $2;"                                                                                   \
                     "ctc2 $14, $3;"                                                                                   \
                     "ctc2 $15, $4"                                                                                    \
        :                                                                                                              \
        : "r"(r)                                                                                                       \
        : "$12", "$13", "$14", "$15", "memory")

/* Load the translation vector (control registers 5-7) from a MATRIX. */
#define gte_SetTransMatrix(r)                                                                                          \
    __asm__ volatile("addu $12, %0, $0;"                                                                               \
                     "lw $13, 20($12);"                                                                                \
                     "lw $14, 24($12);"                                                                                \
                     "ctc2 $13, $5;"                                                                                   \
                     "lw $15, 28($12);"                                                                                \
                     "ctc2 $14, $6;"                                                                                   \
                     "ctc2 $15, $7"                                                                                    \
        :                                                                                                              \
        : "r"(r)                                                                                                       \
        : "$12", "$13", "$14", "$15", "memory")

#endif

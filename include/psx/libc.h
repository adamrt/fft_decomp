#ifndef PSX_LIBC_H
#define PSX_LIBC_H

#include "psx/types.h"

/* random */
extern int rand(void);
extern void srand(unsigned int);

/* values */
extern int abs(int value);

/* strings */
extern char* strcat(char* destination, const char* source);
s32 strcmp(const char* left, const char* right);
extern char* strcpy(char* destination, const char* source);
extern int strlen(const char* source);

/* memory */
extern void* bcopy(const void* source, void* destination, int size);
void* bzero(void* destination, s32 byte_length);
extern void* memchr(const void* source, u8 value, int size);
extern void* memcpy(void* destination, const void* source, unsigned int size);
void* memmove(void* destination, const void* source, s32 count);
extern void* memset(void* destination, u8 value, int size);

/* output */
int printf(const char* fmt, ...);
void puts(const char* text);
extern int sprintf(char* destination, const char* format, ...);

/* jumps */
/* BIOS setjmp saves ra, sp, fp, s0-s7 and gp. */
#define JB_SIZE 12
typedef s32 jmp_buf[JB_SIZE];
s32 setjmp(jmp_buf jump_buffer);

/* driver internals */

typedef enum psyq_printf_flag {
    PSYQ_PRINTF_LEFT = 0x01,
    PSYQ_PRINTF_PLUS = 0x02,
    PSYQ_PRINTF_ALTERNATE = 0x04,
    PSYQ_PRINTF_ZERO = 0x08,
    PSYQ_PRINTF_PRECISION = 0x10,
    PSYQ_PRINTF_SHORT = 0x20,
    PSYQ_PRINTF_LONG = 0x40,
    PSYQ_PRINTF_CAPITAL_L = 0x80
} psyq_printf_flag_e;

/* The formatter changes flags as a 32-bit value and the sign through byte one. */
typedef struct {
    union {
        u32 bits;
        struct {
            u8 flags;
            u8 sign;
            u8 _unused_02[2];
        } bytes;
    } flags;
    s32 width;
    s32 precision;
} psyq_printf_format_t;

extern const psyq_printf_format_t g_psyq_libc_default_format;
extern const char g_psyq_libc_uppercase_digits[];
extern const char g_psyq_libc_lowercase_digits[];

#endif

#ifndef PSX_BIOS_H
#define PSX_BIOS_H

#include "psx/types.h"

/* Selectors used by linked retail veneers and exception-table patches. */
typedef enum {
    PSYQ_BIOS_TABLE_A = 0xa0,
    PSYQ_BIOS_TABLE_B = 0xb0,
    PSYQ_BIOS_TABLE_C = 0xc0,
} psyq_bios_table_e;

typedef enum {
    PSYQ_BIOS_A_SETJMP = 0x13,
    PSYQ_BIOS_A_STRCAT = 0x15,
    PSYQ_BIOS_A_STRCMP = 0x17,
    PSYQ_BIOS_A_STRCPY = 0x19,
    PSYQ_BIOS_A_STRLEN = 0x1b,
    PSYQ_BIOS_A_BCOPY = 0x27,
    PSYQ_BIOS_A_BZERO = 0x28,
    PSYQ_BIOS_A_MEMCPY = 0x2a,
    PSYQ_BIOS_A_MEMSET = 0x2b,
    PSYQ_BIOS_A_MEMCHR = 0x2e,
    PSYQ_BIOS_A_RAND = 0x2f,
    PSYQ_BIOS_A_SRAND = 0x30,
    PSYQ_BIOS_A_INIT_HEAP = 0x39,
    PSYQ_BIOS_A_PRINTF = 0x3f,
    PSYQ_BIOS_A_FLUSH_CACHE = 0x44,
    PSYQ_BIOS_A_GPU_CW = 0x49,
    PSYQ_BIOS_A_BU_INIT = 0x70,
    PSYQ_BIOS_A_96_REMOVE = 0x72,
    PSYQ_BIOS_A_SET_MEM = 0x9f,
    PSYQ_BIOS_A_CARD_INFO = 0xab,
    PSYQ_BIOS_A_CARD_LOAD = 0xac,
    PSYQ_BIOS_A_CARD_AUTO = 0xad,
} psyq_bios_a_service_e;

typedef enum {
    PSYQ_BIOS_B_DELIVER_EVENT = 0x07,
    PSYQ_BIOS_B_OPEN_EVENT = 0x08,
    PSYQ_BIOS_B_CLOSE_EVENT = 0x09,
    PSYQ_BIOS_B_WAIT_EVENT = 0x0a,
    PSYQ_BIOS_B_TEST_EVENT = 0x0b,
    PSYQ_BIOS_B_ENABLE_EVENT = 0x0c,
    PSYQ_BIOS_B_DISABLE_EVENT = 0x0d,
    PSYQ_BIOS_B_STOP_PAD = 0x14,
    PSYQ_BIOS_B_PAD_INIT2 = 0x15,
    PSYQ_BIOS_B_PAD_DR = 0x16,
    PSYQ_BIOS_B_RETURN_FROM_EXCEPTION = 0x17,
    PSYQ_BIOS_B_RESET_ENTRY_INT = 0x18,
    PSYQ_BIOS_B_HOOK_ENTRY_INT = 0x19,
    PSYQ_BIOS_B_FILE_OPEN = 0x32,
    PSYQ_BIOS_B_FILE_SEEK = 0x33,
    PSYQ_BIOS_B_FILE_READ = 0x34,
    PSYQ_BIOS_B_FILE_WRITE = 0x35,
    PSYQ_BIOS_B_FILE_CLOSE = 0x36,
    PSYQ_BIOS_B_PUTS = 0x3f,
    PSYQ_BIOS_B_FILE_FORMAT = 0x41,
    PSYQ_BIOS_B_FIRSTFILE = 0x42,
    PSYQ_BIOS_B_NEXTFILE = 0x43,
    PSYQ_BIOS_B_FILE_DELETE = 0x45,
    PSYQ_BIOS_B_INIT_CARD2 = 0x4a,
    PSYQ_BIOS_B_START_CARD2 = 0x4b,
    PSYQ_BIOS_B_STOP_CARD2 = 0x4c,
    PSYQ_BIOS_B_CARD_WRITE = 0x4e,
    PSYQ_BIOS_B_NEW_CARD = 0x50,
    PSYQ_BIOS_B_FILE_GET_ERROR = 0x55,
    PSYQ_BIOS_B_GET_C0_TABLE = 0x56,
    PSYQ_BIOS_B_GET_B0_TABLE = 0x57,
    PSYQ_BIOS_B_CHANGE_CLEAR_PAD = 0x5b,
    PSYQ_BIOS_B_CARD_STATUS = 0x5c,
} psyq_bios_b_service_e;

typedef enum {
    PSYQ_BIOS_C_CHANGE_CLEAR_RCNT = 0x0a,
} psyq_bios_c_service_e;

typedef enum {
    PSYQ_BIOS_SYSCALL_ENTER_CRITICAL_SECTION = 1,
    PSYQ_BIOS_SYSCALL_EXIT_CRITICAL_SECTION = 2,
} psyq_bios_syscall_e;

/* View of the BIOS C0 dispatch table returned by B(0x56): only entry 6, the
 * exception handler the libcard and libgte patches rewrite, is dereferenced. */
typedef struct {
    void* _unused_00[6];
    u32* exception_handler;
} psyq_bios_c0_table_t;

#endif

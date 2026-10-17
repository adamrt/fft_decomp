#include "psx/types.h"

/* Reserved architectural state; declarations precede headers with C inline definitions. */
register u32 psyq_crt_stack_pointer __asm__("$29");
register u32 psyq_crt_global_pointer __asm__("$28");
register u32 psyq_crt_frame_pointer __asm__("$30");
register void* psyq_crt_return_address __asm__("$31");

#include "psx/crt.h"

/* Focused terminal entry ABI: main should not return; BREAK1 is its retail guard. */
#define PSYQ_CRT_ENTER_GAME(callee)                                                                                    \
    __asm__ volatile(".set\tnoreorder\n\t.macro j architectural_ra\n\t.ifnc \\architectural_ra,$31\n\t"                \
                     ".error \"CRT entry requires architectural terminator\"\n\t.endif\n\tjal %0\n\tnop\n\t"           \
                     "break 1\n\t.purgem j\n\t.endm\n\t"                                                               \
        :                                                                                                              \
        : "i"(callee)                                                                                                  \
        : "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24",       \
        "$25", "hi", "lo", "memory")
#define PSYQ_CRT_ENTER_GAME_END() __asm__(".set\treorder")
#include "psx/cpu_abi_inline.h"

#define PSYQ_CRT_CACHED_SEGMENT 0x80000000

/* main 0x80010a30–0x80010ad0: zero BSS, establish stack/heap/GP and enter main. */
void __SN_ENTRY_POINT(void) {
    /* Pins preserve the original scratch registers and call ABI. */
    u32* cursor;
    u32* end;
    register u32 ram_top __asm__("$2");
    register u32 stack_size __asm__("$3");
    register u32 cached_segment __asm__("$8");
    u32 heap_address;
    register u32 heap_size __asm__("$5");
    register s32 more __asm__("$1");
    cursor = &g_psyq_crt_saved_ra;
    end = (u32*)g_psyq_crt_bss_end;
clear_bss:
    *cursor = 0;
    cursor++;
    more = (u32)cursor < (u32)end;
    if (more != 0)
        goto clear_bss;
    ram_top = _ramsize;
    PSYQ_CPU_LOAD_WAIT();
    PSYQ_CPU_TRAP_ADDI(ram_top, ram_top, -8);
    cached_segment = PSYQ_CRT_CACHED_SEGMENT;
    psyq_crt_stack_pointer = ram_top | cached_segment;
    heap_address = (u32)g_psyq_crt_bss_end;
    heap_address <<= 3;
    heap_address >>= 3;
    stack_size = _stacksize;
    PSYQ_CPU_LOAD_WAIT();
    heap_size = ram_top - stack_size;
    heap_size -= heap_address;
    g_psyq_crt_heap_size = heap_size;
    __asm__ volatile("" : : : "memory"); /* Store the physical heap size before restoring the cached address bit. */
    heap_address |= cached_segment;
    g_psyq_crt_heap_base = (void*)heap_address;
    PSYQ_CPU_GLOBAL_SAVE_RA(g_psyq_crt_saved_ra);
    psyq_crt_global_pointer = (u32)&g_psyq_crt_saved_ra;
    psyq_crt_frame_pointer = psyq_crt_stack_pointer;
    PSYQ_CPU_GLOBAL_RESTORE_RA(g_psyq_crt_saved_ra);
    PSYQ_CRT_ENTER_GAME(main);
    goto* psyq_crt_return_address;
}
PSYQ_CRT_ENTER_GAME_END();

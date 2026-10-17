#include "psx/crt.h"

/* Reserved architectural state; declarations precede headers with C inline definitions. */
register psyq_crt_dispatch_frame_t* psyq_crt_stack __asm__("$29");
register u32 psyq_crt_s0 __asm__("$16");
register u32 psyq_crt_s1 __asm__("$17");
register void* psyq_crt_return_address __asm__("$31");

#include "psx/cpu_abi_inline.h"

/* main 0x80010ad0–0x80010b40: guarded constructor dispatch with a private frame. */
void __main(void) {
    /* Pins preserve the original scratch registers and call ABI. */
    register s32 initialized __asm__("$8");
    psyq_crt_handler_t handler;
    initialized = g_psyq_crt_constructors_ran;
    psyq_crt_stack--;
    psyq_crt_stack->saved_s0 = psyq_crt_s0;
    psyq_crt_stack->saved_s1 = psyq_crt_s1;
    psyq_crt_stack->saved_ra = psyq_crt_return_address;
    __asm__ volatile("" : : : "memory"); /* Keep all private frame saves ahead of the guard branch. */
    if (initialized != 0)
        goto dispatch_done;
    initialized = 1;
    g_psyq_crt_constructors_ran = initialized;
    psyq_crt_s0 = (u32)g_psyq_crt_constructor_table;
    psyq_crt_s1 = (u32)g_psyq_crt_constructor_count;
    if (psyq_crt_s1 == 0)
        goto dispatch_done;
dispatch_next:
    handler = *(psyq_crt_handler_t*)psyq_crt_s0;
    psyq_crt_s0 += sizeof(psyq_crt_handler_t);
    PSYQ_CRT_DISPATCH_CALL(handler);
    psyq_crt_s1--;
    PSYQ_CPU_SHARED_DELAY_END();
    if (psyq_crt_s1 != 0)
        goto dispatch_next;
dispatch_done:
    psyq_crt_return_address = psyq_crt_stack->saved_ra;
    psyq_crt_s1 = psyq_crt_stack->saved_s1;
    psyq_crt_s0 = psyq_crt_stack->saved_s0;
    psyq_crt_stack++;
    __asm__("" : "=r"(psyq_crt_stack) : "0"(psyq_crt_stack)); /* The private frame is popped before the return jump. */
    goto* psyq_crt_return_address;
}

#include "psx/types.h"

/* Hand-assembled in the retail binary: the loads go through $at as a base
 * register (`lui at / move at,at / lw t0,off(at)`), use $t0/$t1 scratch and a
 * trapping `add`, none of which GCC emits. The $1 pin is therefore required.
 * The target uses $at as an ordinary scratch register here, which the assembler
 * warns about by default ("used $at without .set noat"). The directive emits no
 * instructions, so it silences the warning without affecting the output. */
__asm__(".set noat");

s32 world_thread_is_previous_running(void) {
    register s32 offset __asm__("$8");
    register void* global_pointer __asm__("$1") = (void*)0x801d0000;
    register u8* thread_array __asm__("$9");

    __asm__("move $1,$1" : "=r"(global_pointer) : "0"(global_pointer));
    offset = *(s32*)((u8*)global_pointer - 0x2e90);
    __asm__("nop\naddi $8,$8,-1\nsll $8,$8,10" : "=r"(offset) : "0"(offset));
    global_pointer = (void*)0x80150000;
    __asm__("move $1,$1" : "=r"(global_pointer) : "0"(global_pointer));
    thread_array = *(u8**)((u8*)global_pointer + 0x327c);
    __asm__("nop\nadd $8,$8,$9" : "=r"(offset) : "0"(offset), "r"(thread_array));
    return *(s32*)(offset + 0x48);
}

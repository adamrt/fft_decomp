#include "fft/world.h"
#include "psx/types.h"

/* Hand-assembled in the retail binary: the loads go through $at as a base
 * register (`lui at / move at,at / lw t0,off(at)`), use $t0/$t1 scratch and a
 * trapping `add`, none of which GCC emits. The $1 pin is therefore required.
 * The target uses $at as an ordinary scratch register here, which the assembler
 * warns about by default ("used $at without .set noat"). The directive emits no
 * instructions, so it silences the warning without affecting the output. */
__asm__(".set noat");

/* Return parameter 1 from the currently selected WORLD thread record. */
void* world_thread_get_current_parameter_1(void) {
    register void* global_pointer __asm__("$1");
    register s32 thread_offset __asm__("$8");
    register u8* thread_array __asm__("$9");

    global_pointer = (void*)0x801d0000;
    __asm__("move $1,$1" : "=r"(global_pointer) : "0"(global_pointer));
    /* Raw: $at holds a fabricated lui base, not a struct; the displacements
     * address the current-thread id and the g_world_threads pointer. */
    thread_offset = *(s32*)((u8*)global_pointer - 0x2e90);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    global_pointer = (void*)0x80150000;
    __asm__("move $1,$1" : "=r"(global_pointer) : "0"(global_pointer));
    thread_array = *(u8**)((u8*)global_pointer + 0x327c);
    __asm__("nop\nadd $8,$8,$9" : "=r"(thread_offset) : "0"(thread_offset), "r"(thread_array));
    return (void*)((native_thread_t*)thread_offset)->function_parameter_1;
}

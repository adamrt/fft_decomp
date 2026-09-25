#include "fft/battle.h"
#include "psx/types.h"

/* Hand-assembled in the retail binary, like the WORLD siblings
 * (src/world/world_thread_get_current_parameter_1.c): the loads go through $at
 * as a base register (`lui at / addu at,at,zero / lw t0,off(at)`) and use
 * $t0/$t1 scratch, none of which GCC emits. The $1 pin is therefore required.
 * The directive emits no instructions; it silences the "$at without .set
 * noat" warning. */
__asm__(".set noat");

/* Return parameter 1 of the currently selected event thread record. */
s32 battle_thread_get_current_parameter_1(void) {
    register void* global_pointer __asm__("$1");
    register s32 thread_offset __asm__("$8");
    register u8* thread_array __asm__("$9");

    global_pointer = (void*)0x80170000;
    __asm__("addu $1,$1,$zero" : "=r"(global_pointer) : "0"(global_pointer));
    /* Raw: $at holds a fabricated lui base, not a struct; 0x4038/0x5f98 are the
     * low halves of g_battle_current_thread_id and g_battle_threads. */
    thread_offset = *(s32*)((u8*)global_pointer + 0x4038);
    __asm__("nop\nsll $8,$8,10" : "=r"(thread_offset) : "0"(thread_offset));
    global_pointer = (void*)0x80160000;
    __asm__("addu $1,$1,$zero" : "=r"(global_pointer) : "0"(global_pointer));
    thread_array = *(u8**)((u8*)global_pointer + 0x5f98);
    __asm__("nop\naddu $8,$8,$9" : "=r"(thread_offset) : "0"(thread_offset), "r"(thread_array));
    return ((native_thread_t*)thread_offset)->function_parameter_1;
}

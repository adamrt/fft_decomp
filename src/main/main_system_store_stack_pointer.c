/* Storing the caller's stack pointer for restoring the game loop's stack. */

#include "fft/main.h"
#include "psx/types.h"

/* Store the caller's stack pointer in *destination, for
 * main_restore_game_loop_stack_pointer. */
void main_system_store_stack_pointer(u32* destination) {
    /* volatile keeps the store out of the `jr ra` delay slot, which the target leaves as a nop. */
    register volatile u32* output __asm__("$8") = destination;
    register u32 stack_pointer __asm__("$29");

    /* Keeps the `move t0,a0`; without this use GCC stores through $a0. */
    __asm__("" : : "r"(output));
    *output = stack_pointer;
}

#include "fft/event.h"
#include "fft/world.h"

/* Tutorial command: start the text thread on a new message. Operands: two
 * message-id bytes (little-endian, biased by 0xB800) and a third byte stored
 * into the thread block at +0x405C.
 *
 * -O0, like the neighbouring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. */
void world_script_handle_tutorial_command_display_message(void) {
    world_thread_start(0x10, world_text_character_handling_thread);
    world_thread_set_parameters(0x10, 0x99,
        (s32)((u8*)(g_world_script_tutorial_command_ptr[2] * 0x100)
            + (g_world_script_tutorial_command_ptr[1] + 0xB800)),
        0);
    /* The raw byte displacement preserves the target's single +0x405c address
     * calculation; the equivalent typed member access splits it at -O0. */
    *(s32*)((u8*)g_world_threads + 0x405C) = g_world_script_tutorial_command_ptr[3];
    g_world_script_tutorial_command_ptr += 4;
}

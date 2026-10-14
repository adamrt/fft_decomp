#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Tutorial command: replace the dialog text once the current text has
 * finished printing. Operands: two message-id bytes (little-endian, biased
 * by 0xB800) that are handed to the text thread as its second parameter.
 *
 * -O0, like the neighboring tutorial handlers. The message id is summed as
 * pointer arithmetic on purpose: written as plain integer addition, GCC
 * 2.6.3's fold re-associates `hi + (lo + 0xB800)` into `(hi + 0xB800) + lo`,
 * and the target adds the bias to the low byte first. */
/* Address-of operand fields preserves the -O0 byte-address steps. */
void world_script_handle_tutorial_command_change_dialog(void) {
    if (world_text_is_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT) != 1) {
        thread_set_parameters(WORLD_THREAD_SLOT_TUTORIAL_TEXT, 0x99,
            (s32)((u8*)(*(&((world_tutorial_dialog_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[1])
                      * 0x100)
                + (*(&((world_tutorial_dialog_instruction_t*)g_world_script_tutorial_command_ptr)->text_id_le[0])
                    + 0xB800)),
            0);
        g_world_script_tutorial_command_ptr += 3;
        /* Direct slot/member indexing splits this address at -O0. */
        *(s32*)((u8*)g_threads + (u32) & ((thread_t*)0)[WORLD_THREAD_SLOT_TUTORIAL_TEXT].task_id) = THREAD_TASK_RESUME;
    }
}

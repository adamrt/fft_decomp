/* Running a menu command stream up to its terminator. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Twin of menu_cmd_run_stream_variant; BUNIT's stream terminator is 0x1C. */
void menu_cmd_run_stream_variant(menu_instruction_t* instruction, s32 flags) {
    g_menu_gfx_otag_index = 0;
    g_menu_gfx_semitransparency = 0;
    g_menu_cmd_stream_input = flags;
    g_gfx_otag_index_locked = 0;
    if (instruction->header.opcode != VARIANT_MENU_CMD_END_OPCODE) {
        do {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
        } while (instruction->header.opcode != VARIANT_MENU_CMD_END_OPCODE);
    }
}

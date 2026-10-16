/* Running a menu command stream up to its terminator. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Run a menu command stream: reset the ordering-table index, semi-transparency and lock, keep
 * `flags` as the stream's input, then dispatch each command through g_menu_cmd_handlers until
 * the program's end opcode (VARIANT_MENU_CMD_END_OPCODE: 0x1C in BUNIT and WORLD, 0x19 in
 * EQUIP). */
void menu_cmd_run_stream(menu_instruction_t* instruction, s32 flags) {
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

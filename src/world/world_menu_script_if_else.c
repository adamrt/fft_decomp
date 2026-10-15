#include "fft/world.h"
#include "psx/types.h"

/* Execute the selected branch and skip the other by encoded instruction
 * lengths, using a literal predicate argument or the current scrolling row. */
menu_instruction_t* world_menu_script_if_else(menu_instruction_t* instruction) {
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[instruction->if_else.predicate].value;
    s32 count_true = instruction->if_else.count_true;
    s32 count_false = instruction->if_else.count_false;
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = instruction->if_else.value;
    } else {
        arg = g_world_menu_scroll_offset + g_world_menu_scroll_row_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            arg -= 1;
        }
    }
    instruction = menu_next_instruction(instruction);
    if (predicate(arg) != 0) {
        count_true -= 1;
        while (count_true != -1) {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            instruction = menu_next_instruction(instruction);
            count_false -= 1;
        }
    } else {
        count_true -= 1;
        while (count_true != -1) {
            instruction = menu_next_instruction(instruction);
            count_true -= 1;
        }
        count_false -= 1;
        while (count_false != -1) {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
            count_false -= 1;
        }
    }
    return instruction;
}

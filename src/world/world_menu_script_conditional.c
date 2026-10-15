#include "fft/world.h"
#include "psx/types.h"

/* Execute or skip the following instruction group according to the selected
 * predicate, using a literal argument or the current scrolling row. */
menu_instruction_t* world_menu_script_conditional(menu_instruction_t* script) {
    menu_instruction_t* instruction = script;
    s32 (*predicate)(s32) = g_world_menu_script_callbacks[instruction->conditional.predicate].value;
    s32 count = instruction->conditional.count;
    s32 arg;

    if (g_world_menu_use_scroll_position == 0) {
        arg = instruction->conditional.value;
    } else {
        arg = g_menu_scroll_base_index + g_world_menu_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
        }
    }
    instruction = menu_next_instruction(instruction);
    if (predicate(arg) == 0) {
        count -= 1;
        while (count != -1) {
            instruction = menu_next_instruction(instruction);
            count -= 1;
        }
    } else {
        count -= 1;
        while (count != -1) {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
            count -= 1;
        }
    }
    return instruction;
}

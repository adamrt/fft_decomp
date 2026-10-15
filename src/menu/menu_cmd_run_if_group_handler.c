#include "fft/menu_script.h"

/* Run the following group if its predicate succeeds for the literal or
 * current list row; otherwise skip it. */
menu_instruction_t* menu_cmd_run_if_group_handler(menu_instruction_t* cmd) {
    s32 (*callback)(s32);
    s32 count;
    s32 row;

    callback = g_menu_row_callbacks[cmd->conditional.predicate].value;
    count = cmd->conditional.count;
    if (g_menu_list_row_mode == 0) {
        row = cmd->conditional.value;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row -= 1;
        }
    }
    cmd = menu_next_instruction(cmd);
    if (callback(row) == 0) {
        while (--count != -1) {
            cmd = menu_next_instruction(cmd);
        }
    } else {
        while (--count != -1) {
            cmd = g_menu_cmd_handlers[cmd->header.opcode](cmd);
        }
    }
    return cmd;
}

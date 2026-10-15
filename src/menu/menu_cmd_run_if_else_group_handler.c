#include "fft/menu_script.h"

/* Run the selected branch and skip the other, using the literal or current
 * list row as the predicate's argument. */
menu_instruction_t* menu_cmd_run_if_else_group_handler(menu_instruction_t* cmd) {
    s32 then_count;
    s32 else_count;
    s32 (*callback)(s32);
    s32 row;

    then_count = cmd->if_else.count_true;
    else_count = cmd->if_else.count_false;
    callback = g_menu_row_callbacks[cmd->if_else.predicate].value;
    if (g_menu_list_row_mode == 0) {
        row = cmd->if_else.value;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        if (g_menu_list_scroll_offset < 0) {
            row--;
        }
    }
    cmd = menu_next_instruction(cmd);
    if (callback(row) != 0) {
        for (then_count--; then_count != -1; then_count--) {
            cmd = g_menu_cmd_handlers[cmd->header.opcode](cmd);
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd = menu_next_instruction(cmd);
        }
    } else {
        for (then_count--; then_count != -1; then_count--) {
            cmd = menu_next_instruction(cmd);
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd = g_menu_cmd_handlers[cmd->header.opcode](cmd);
        }
    }
    return cmd;
}

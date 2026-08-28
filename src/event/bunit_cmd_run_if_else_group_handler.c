#include "fft/bunit.h"
#include "psx/types.h"

/* Run the next `then_count` commands and skip the following `else_count`
 * when row callback cmd[3] returns nonzero; otherwise skip the first group
 * and run the second. */
u8* bunit_cmd_run_if_else_group_handler(u8* cmd) {
    s32 then_count;
    s32 else_count;
    s32 (*callback)(s32);
    s32 row;

    then_count = cmd[4];
    else_count = cmd[5];
    callback = g_bunit_menu_row_callbacks[cmd[3]];
    if (g_bunit_menu_scroll_list_active == 0) {
        row = cmd[6];
    } else {
        row = g_bunit_menu_scroll_base_index + g_bunit_menu_cursor_row;
        if (g_bunit_menu_scroll_pixel_offset < 0) {
            row--;
        }
    }
    cmd += cmd[1];
    if (callback(row) != 0) {
        for (then_count--; then_count != -1; then_count--) {
            cmd = g_bunit_cmd_handlers[*cmd](cmd);
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd += cmd[1];
        }
    } else {
        for (then_count--; then_count != -1; then_count--) {
            cmd += cmd[1];
        }
        for (else_count--; else_count != -1; else_count--) {
            cmd = g_bunit_cmd_handlers[*cmd](cmd);
        }
    }
    return cmd;
}

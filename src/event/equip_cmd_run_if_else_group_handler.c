#include "fft/event_equip.h"
#include "psx/types.h"

u8* equip_cmd_run_if_else_group_handler(u8* command) {
    s32 then_count;
    s32 else_count;
    s32 (*handler)(s32);
    s32 idx;

    then_count = command[4];
    else_count = command[5];
    handler = g_equip_menu_list_row_callbacks[command[3]];
    if (g_equip_menu_list_row_mode == 0) {
        idx = command[6];
    } else {
        idx = g_equip_menu_scroll_base_index + g_equip_menu_list_row_index;
        if (g_equip_menu_list_scroll_offset < 0) {
            idx--;
        }
    }
    command += command[1];
    if (handler(idx) != 0) {
        for (then_count--; then_count != -1; then_count--) {
            command = g_equip_cmd_handlers[*command](command);
        }
        for (else_count--; else_count != -1; else_count--) {
            command += command[1];
        }
    } else {
        for (then_count--; then_count != -1; then_count--) {
            command += command[1];
        }
        for (else_count--; else_count != -1; else_count--) {
            command = g_equip_cmd_handlers[*command](command);
        }
    }
    return command;
}

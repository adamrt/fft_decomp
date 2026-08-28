#include "fft/equip.h"
#include "psx/types.h"

u8* equip_cmd_run_if_group_handler(u8* command) {
    s32 (*handler)(s32);
    s32 count;
    s32 arg;

    handler = g_equip_menu_list_row_callbacks[command[3]];
    count = command[4];
    if (g_equip_menu_list_row_mode == 0) {
        arg = command[5];
    } else {
        arg = g_equip_menu_scroll_base_index + g_equip_menu_list_row_index;
        if (g_equip_menu_list_scroll_offset < 0) {
            arg = arg - 1;
        }
    }

    command += command[1];
    if (handler(arg) == 0) {
        count--;
        while (count != -1) {
            command += command[1];
            count--;
        }
        return command;
    }
    count--;
    while (count != -1) {
        command = g_equip_cmd_handlers[command[0]](command);
        count--;
    }
    return command;
}

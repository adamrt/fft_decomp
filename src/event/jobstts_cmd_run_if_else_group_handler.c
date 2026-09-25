#include "fft/event_jobstts.h"
#include "psx/types.h"

u8* jobstts_cmd_run_if_else_group_handler(u8* data) {
    s32 then_count;
    s32 else_count;
    s32 arg;
    jobstts_condition_t condition;

    then_count = data[4];
    else_count = data[5];
    condition = g_jobstts_cmd_conditions[data[3]];

    if (g_jobstts_menu_list_row_rendering == 0) {
        arg = data[6];
    } else {
        arg = g_jobstts_menu_scroll_base_index + g_jobstts_menu_list_row_index;
        if (g_jobstts_menu_list_scroll_offset_y < 0) {
            arg--;
        }
    }

    data += data[1];
    if (condition(arg)) {
        then_count -= 1;
        while (then_count != -1) {
            data = g_jobstts_cmd_handlers[data[0]](data);
            then_count -= 1;
        }
        else_count -= 1;
        while (else_count != -1) {
            data += data[1];
            else_count -= 1;
        }
    } else {
        then_count -= 1;
        while (then_count != -1) {
            data += data[1];
            then_count -= 1;
        }
        else_count -= 1;
        while (else_count != -1) {
            data = g_jobstts_cmd_handlers[data[0]](data);
            else_count -= 1;
        }
    }
    return data;
}

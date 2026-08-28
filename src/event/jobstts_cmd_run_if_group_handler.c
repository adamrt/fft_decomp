#include "fft/jobstts.h"
#include "psx/types.h"

u8* jobstts_cmd_run_if_group_handler(u8* data) {
    s32 count;
    jobstts_condition_t condition;
    s32 index;

    count = data[4];
    condition = g_jobstts_cmd_conditions[data[3]];
    if (g_jobstts_menu_list_row_rendering == 0) {
        index = data[5];
    } else {
        index = g_jobstts_menu_scroll_base_index + g_jobstts_menu_list_row_index;
        if (g_jobstts_menu_list_scroll_offset_y < 0) {
            index -= 1;
        }
    }
    data += data[1];
    if (condition(index) == 0) {
        count -= 1;
        while (count != -1) {
            data += data[1];
            count -= 1;
        }
        return data;
    }
    count -= 1;
    while (count != -1) {
        data = g_jobstts_cmd_handlers[data[0]](data);
        count -= 1;
    }
    return data;
}

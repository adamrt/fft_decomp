#include "fft/event_jobstts.h"
#include "psx/types.h"

/* Run the following command group if the row or literal predicate succeeds;
 * otherwise skip it. */
menu_instruction_t* jobstts_cmd_run_if_group_handler(menu_instruction_t* data) {
    s32 count;
    jobstts_condition_t condition;
    s32 index;

    count = data->conditional.count;
    condition = g_jobstts_cmd_conditions[data->conditional.predicate];
    if (g_jobstts_menu_list_row_rendering == 0) {
        index = data->conditional.value;
    } else {
        index = g_menu_scroll_base_index + g_jobstts_menu_list_row_index;
        if (g_jobstts_menu_list_scroll_offset_y < 0) {
            index -= 1;
        }
    }
    data = menu_next_instruction(data);
    if (condition(index) == 0) {
        count -= 1;
        while (count != -1) {
            data = menu_next_instruction(data);
            count -= 1;
        }
        return data;
    }
    count -= 1;
    while (count != -1) {
        data = g_jobstts_cmd_handlers[data->header.opcode](data);
        count -= 1;
    }
    return data;
}

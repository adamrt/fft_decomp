#include "fft/event_jobstts.h"
#include "psx/types.h"

/* Run the selected branch and skip the other, using the current row or
 * literal operand as the predicate's argument. */
menu_instruction_t* jobstts_cmd_run_if_else_group_handler(menu_instruction_t* data) {
    s32 then_count;
    s32 else_count;
    s32 arg;
    jobstts_condition_t condition;

    then_count = data->if_else.count_true;
    else_count = data->if_else.count_false;
    condition = g_jobstts_cmd_conditions[data->if_else.predicate];

    if (g_jobstts_menu_list_row_rendering == 0) {
        arg = data->if_else.value;
    } else {
        arg = g_menu_scroll_base_index + g_jobstts_menu_list_row_index;
        if (g_jobstts_menu_list_scroll_offset_y < 0) {
            arg--;
        }
    }

    data = menu_next_instruction(data);
    if (condition(arg)) {
        then_count -= 1;
        while (then_count != -1) {
            data = g_jobstts_cmd_handlers[data->header.opcode](data);
            then_count -= 1;
        }
        else_count -= 1;
        while (else_count != -1) {
            data = menu_next_instruction(data);
            else_count -= 1;
        }
    } else {
        then_count -= 1;
        while (then_count != -1) {
            data = menu_next_instruction(data);
            then_count -= 1;
        }
        else_count -= 1;
        while (else_count != -1) {
            data = g_jobstts_cmd_handlers[data->header.opcode](data);
            else_count -= 1;
        }
    }
    return data;
}

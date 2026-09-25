#include "fft/battle.h"
#include "psx/types.h"

s32 battle_menu_should_close_thread(s32* out_flag) {
    s32 i;
    s32 offset;
    s32 task_state;

    if (g_event_mode != 0 || g_battle_menu_help_open != 0) {
        *out_flag = 0;
        return 0;
    }
    i = 0;
    task_state = 3;
    offset = 0;
    for (; i < 16; i++, offset += NATIVE_THREAD_STRIDE) {
        if (i != g_battle_current_thread_id && battle_thread_is_running_8014cc94(i) != 0
            && *(volatile s32*)((s8*)g_battle_thread_task_ids + offset) == task_state) {
            *out_flag = 0;
            break;
        }
    }
    if (i == 16 && battle_thread_get_current_parameter_3() != 0) {
        return 1;
    }
    return 0;
}

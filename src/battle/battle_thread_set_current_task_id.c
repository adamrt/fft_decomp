#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_set_current_task_id(s32 task_id) {
    g_battle_thread_task_ids[g_battle_current_thread_id][0] = task_id;
}

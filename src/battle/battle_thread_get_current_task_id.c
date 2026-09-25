#include "fft/battle.h"
#include "psx/types.h"

s32 battle_thread_get_current_task_id(void) {
    return g_battle_thread_task_ids[g_battle_current_thread_id][0];
}

#include "fft/battle.h"
#include "psx/types.h"

void battle_script_waitspritemove_event_instruction(s32 unit_id) {
    s32 target;
    s32 i;

    target = battle_get_misc_id(unit_id);
    if (target == 2000) {
        return;
    }

    do {
        for (i = 0; i < 16; i++) {
            if (battle_thread_is_running_8014cc94(i) != 0) {
                if (g_battle_thread_task_ids[i][0] == NATIVE_THREAD_TASK_SPRITE_MOVE) {
                    if (g_battle_thread_task_ids[i][1] == target) {
                        break;
                    }
                }
            }
        }
        if (i == 16) {
            return;
        }
        battle_thread_yield();
    } while (1);
}

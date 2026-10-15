#include "fft/battle_script.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* WaitSpriteMove: yield until no running thread carries the sprite-move task for
 * this unit's unit id. A unit id that does not resolve has nothing to wait for.
 * BATTLE scans thread slots 0..15; WORLD's copy scans 0..16. */
void script_waitspritemove_event_instruction(s32 unit_id) {
    s32 target;
    s32 i;

    target = script_resolve_runtime_id(unit_id);
    if (target == EVENT_RUNTIME_ID_NONE) {
        return;
    }

    do {
        for (i = 0; i < 16; i++) {
            if (thread_is_running_asm_variant(i) != 0) {
                if (g_thread_contexts[i].task_id == THREAD_TASK_SPRITE_MOVE) {
                    if (g_thread_contexts[i].task_words[0] == target) {
                        break;
                    }
                }
            }
        }
        if (i == 16) {
            return;
        }
        thread_yield();
    } while (1);
}

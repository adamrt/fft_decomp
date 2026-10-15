#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* WaitSpriteMove: yield until no thread is still running the sprite-move task
 * for the calling event's sprite. A unit id that does not resolve has nothing
 * to wait for. */
void script_waitspritemove_event_instruction(s32 unit_id) {
    s32 runtime_id;
    s32 i;

    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    for (;;) {
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm_variant(i) != 0 && g_thread_contexts[i].task_id == THREAD_TASK_SPRITE_MOVE
                && g_thread_contexts[i].task_words[0] == runtime_id) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            return;
        }
        thread_yield();
    }
}

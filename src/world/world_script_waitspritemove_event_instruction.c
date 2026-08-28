#include "fft/event.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Block until no thread is still running the sprite-move task (task 0xB) for
 * the calling event's sprite. A misc id of 0x7d0 means the sprite is absent,
 * so there is nothing to wait for. */
void world_script_waitspritemove_event_instruction(s32 unit_id) {
    s32 misc_id;
    s32 i;

    misc_id = world_get_misc_id(unit_id);
    if (misc_id == EVENT_MISC_ID_NONE) {
        return;
    }
    for (;;) {
        for (i = 0; i < 17; i++) {
            if (world_thread_is_running_80100164(i) != 0
                && g_world_thread_contexts[i].task_id == NATIVE_THREAD_TASK_SPRITE_MOVE
                && g_world_thread_contexts[i].task_words[0] == misc_id) {
                break;
            }
        }
        if (i == 17) {
            return;
        }
        world_thread_yield();
    }
}

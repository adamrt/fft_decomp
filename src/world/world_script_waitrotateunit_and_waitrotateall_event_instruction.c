#include "fft/world.h"
#include "psx/types.h"

/* WaitRotateUnit / WaitRotateAll: yield until the named unit, or every unit
 * when the id is -1, has finished rotating. */
void world_script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id) {
    s32 i;
    s32 misc_id;

    if (unit_id == -1) {
        do {
            world_thread_yield();
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (g_world_unit_animation_states[i].rotating != 0) {
                    break;
                }
            }
        } while (i != EVENT_UNIT_SLOT_COUNT);
    } else {
        misc_id = world_get_misc_id(unit_id);
        if (misc_id != EVENT_MISC_ID_NONE) {
            do {
                world_thread_yield();
            } while (g_world_unit_animation_states[misc_id].rotating != 0);
        }
    }
}

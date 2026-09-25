#include "fft/battle.h"
#include "fft/battle_unit_rotation.h"
#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

void battle_script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id) {
    s32 i;
    s32 index;

    if (unit_id == -1) {
        do {
            battle_thread_yield();
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (g_battle_unit_misc_rotation_data[i].rotating != 0) {
                    break;
                }
            }
        } while (i != EVENT_UNIT_SLOT_COUNT);
    } else {
        index = battle_get_misc_id(unit_id);
        if (index != 0x7D0) {
            do {
                battle_thread_yield();
            } while (g_battle_unit_misc_rotation_data[index].rotating != 0);
        }
    }
}

#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Event instruction 0x29 WaitWalk: yield until the unit stops moving. */
void world_script_waitwalk_event_instruction(s32 unit_id) {
    s32 misc_id;

    misc_id = world_get_misc_id(unit_id);
    if (misc_id != EVENT_MISC_ID_NONE) {
        do {
            world_thread_yield();
        } while (battle_move_is_unit_moving_by_misc_id(misc_id) != 0);
    }
}

#ifndef BATTLE_SCRIPT_H
#define BATTLE_SCRIPT_H

/* Battle scene actions and orchestration invoked by scenario bytecode. */

#include "psx/types.h"

/* interpreter */

/* Returns the event result that the battle state handlers switch on;
 * proven by 0x80078fb4 and 0x800785ac, which both consume it. */

/* input */
extern u32 g_script_saved_event_input;

/* conditions */

/* unit creation and removal */
s32 battle_script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags);

/* unit statuses */

/* camera */

/* map and lighting */

/* graphics and messages */

/* frame hooks */

/* shared with the per-program event interpreter and dialog thread */
void script_earthquake_start(void);

#endif

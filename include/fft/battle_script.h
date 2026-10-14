#ifndef BATTLE_SCRIPT_H
#define BATTLE_SCRIPT_H

/* Battle scene actions and orchestration invoked by scenario bytecode. */

#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"

/* interpreter */
extern s16 g_script_threads_idle;

void script_process_pending_requests(void);
/* Returns the event result that the battle state handlers switch on;
 * proven by 0x80078fb4 and 0x800785ac, which both consume it. */
void script_set_event_speed(s32 speed);

/* input */
extern u32 g_script_saved_event_input;

void script_route_event_input(void);

/* conditions */

s32 script_is_tutorial_event_slot(void);

/* unit creation and removal */
s32 script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags);
void script_add_unit_start_thread(void);

/* unit movement */

void script_waitspritemove_event_instruction(s32 unit_id);
void script_warp_unit(const event_warp_unit_parameters_t* parameters);

/* unit statuses */

/* camera */
void script_earthquake_start(void);

/* map and lighting */

void script_dark_screen_thread(void);

/* graphics and messages */

/* frame hooks */

#endif

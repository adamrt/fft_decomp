#ifndef TURN_H
#define TURN_H

/* Turn order: the CT clock, whose turn it is, Act and Move taken, ending a turn. */

#include "fft/unit.h"
#include "psx/types.h"

/* Modes of turn_clock_run. */
typedef enum turn_clock_mode {
    TURN_CLOCK_MODE_RUN = 0,
    TURN_CLOCK_MODE_RESET = 1,
    TURN_CLOCK_MODE_FIND_ACTIVE_UNIT = 2
} turn_clock_mode_e;

/* g_turn_clock_state and g_turn_clock_resume_state: the phases of
 * turn_clock_run. */
typedef enum turn_clock_state {
    TURN_CLOCK_STATE_ADD_CT = 0,         /* every unit gains CT by its speed */
    TURN_CLOCK_STATE_PICK_UNIT = 1,      /* the highest CT of 100 or more takes a turn */
    TURN_CLOCK_STATE_TICK_CHARGES = 2,   /* count down every charged_ability_ct */
    TURN_CLOCK_STATE_RESOLVE_CHARGE = 3, /* a charge that reached 0 resolves */
    TURN_CLOCK_STATE_EXPIRE_STATUSES = 4,
    TURN_CLOCK_STATE_COLLECT_PENDING_ACTIONS = 5, /* note the units whose action has an attack type */
    TURN_CLOCK_STATE_RUN_PENDING_ACTIONS = 6,
    TURN_CLOCK_STATE_PICK_UNIT_AFTER_CHARGE = 9, /* a unit whose CT is 0xff takes a turn at once */
    TURN_CLOCK_STATE_UNIT_TURN = 10,             /* end the turn of each unit that has one */
    TURN_CLOCK_STATE_MARK_MIMES = 13,
    TURN_CLOCK_STATE_RUN_MIMES = 14,
    TURN_CLOCK_STATE_TERRAIN_POISON = 15,
    TURN_CLOCK_STATE_REMOVE_TRANSPARENT = 17,
    TURN_CLOCK_STATE_POISON_AND_REGEN = 19
} turn_clock_state_e;

/* Returned by turn_status_flags_calculate. */
typedef enum turn_status_flags {
    TURN_STATUS_CT_FROZEN = 0x01,
    TURN_STATUS_INCAPACITATED = 0x02,
    TURN_STATUS_DEAD = 0x04,
    TURN_STATUS_SLEEP = 0x08,
    TURN_STATUS_HASTE = 0x10,
    TURN_STATUS_SLOW = 0x20,
    TURN_STATUS_BLOCKS_WAIT_MENU_MASK = 0x0f,
    TURN_STATUS_BLOCKS_TURN_MASK = 0x0d
} turn_status_flags_e;

typedef enum turn_event {
    TURN_EVENT_UNIT_READY = 0x0100,
    TURN_EVENT_ABILITY_READY = 0x0200,
    TURN_EVENT_ACTION_RESULT = 0x0300,
    TURN_EVENT_UNKNOWN_0400 = 0x0400,
    TURN_EVENT_MIME = 0x0500,
    TURN_EVENT_NONE = 0xff00
} turn_event_e;

/* unit_runtime_t.turn_action_flags. */
typedef enum turn_action_flags { TURN_ACTION_MOVED = 0x01, TURN_ACTION_ACTED = 0x02 } turn_action_flags_e;

/* End of turn: CT gains 20 for an unused move and 20 for an unused action, and
 * the result is capped at 60. */
enum { TURN_UNUSED_CT_REFUND = 20, TURN_END_CT_MAX = 60 };

extern turn_clock_state_e g_turn_clock_state;
extern turn_clock_state_e g_turn_clock_resume_state;
extern s32 g_turn_unit_id;
extern s32 g_turn_event_type; /* turn_event_e */

s32 turn_clock_run(turn_clock_mode_e mode);
turn_status_flags_e turn_status_flags_calculate(const unit_stats_t* unit);
void turn_next_event_start(void);
void turn_move_taken_set(s32 unit_id);
void turn_act_taken_set(s32 unit_id);
s32 turn_is_over(s32 unit_id);
s32 turn_end(s32 unit_id);

#endif

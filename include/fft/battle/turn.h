#ifndef TURN_H
#define TURN_H

/* Turn order: the CT clock, whose turn it is, Act and Move taken, ending a turn, and the turn command a unit carries
 * out next. */

#include "fft/game/battle_unit.h"
#include "fft/map/map.h"
#include "psx/types.h"

/* CT clock */
/* Modes of turn_run_clock. */
typedef enum turn_clock_mode {
    TURN_CLOCK_MODE_RUN = 0,
    TURN_CLOCK_MODE_RESET = 1,
    TURN_CLOCK_MODE_FIND_ACTIVE_UNIT = 2
} turn_clock_mode_e;

/* g_turn_clock_state and g_turn_clock_resume_state: the phases of
 * turn_run_clock. */
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

/* Returned by turn_calculate_status_flags. */
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

extern turn_clock_state_e g_turn_clock_state;
extern turn_clock_state_e g_turn_clock_resume_state;

s32 turn_run_clock(turn_clock_mode_e mode);
turn_status_flags_e turn_calculate_status_flags(const battle_unit_t* unit);

/* turns */
typedef enum turn_event {
    TURN_EVENT_UNIT_READY = 0x0100,
    TURN_EVENT_ABILITY_READY = 0x0200,
    TURN_EVENT_ACTION_RESULT = 0x0300,
    TURN_EVENT_UNKNOWN_0400 = 0x0400,
    TURN_EVENT_MIME = 0x0500,
    TURN_EVENT_NONE = 0xff00
} turn_event_e;

/* map_unit_t.turn_action_flags. */
typedef enum turn_action_flags { TURN_ACTION_MOVED = 0x01, TURN_ACTION_ACTED = 0x02 } turn_action_flags_e;

/* End of turn: CT gains 20 for an unused move and 20 for an unused action, and
 * the result is capped at 60. */
enum { TURN_UNUSED_CT_REFUND = 20, TURN_END_CT_MAX = 60 };

extern s32 g_turn_unit_id;

void turn_set_move_taken(s32 unit_id);
void turn_set_act_taken(s32 unit_id);
s32 turn_is_over(s32 unit_id);
s32 turn_end(s32 unit_id);

/* turn commands */
/* Kinds of turn command, the next thing a unit does: built by the AI or taken from a player's menu choice. */
typedef enum turn_command_kind {
    TURN_COMMAND_MOVE = 0,
    TURN_COMMAND_ACT = 1,
    TURN_COMMAND_END_TURN = 2,
    /* The rest reach the command from command_resolve_selection, for a player's menu choice.
     * The auto-battle choices are 12 plus twice the option, plus one once a unit has been
     * picked for the two that name one (SELECT picks it, DISPLAY shows it); SELECT and the two
     * others equal their ai_autobattle_mode_e values. */
    TURN_COMMAND_NO_MENU_RECORD = 7, /* the current menu id has no record: back to the turn events */
    /* A prompt's answers, from its command map: YES also keeps a move or a choice, NO undoes it,
     * and a prompt closed without an answer leaves UNRESOLVED_FF. */
    TURN_COMMAND_PROMPT_YES = 7,
    TURN_COMMAND_PROMPT_NO = 8,
    TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT = 12,
    TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_DISPLAY = 13,
    TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT = 14,
    TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_DISPLAY = 15,
    TURN_COMMAND_AUTOBATTLE_FADING_LIFE = 16,
    TURN_COMMAND_AUTOBATTLE_RETREAT = 17,
    TURN_COMMAND_REFRESH_CONTROL = 18, /* copy the control flag back from the battle unit */
    TURN_COMMAND_MOVE_SELF = 25,       /* MOVE that never redirects to the unit's mount */
    TURN_COMMAND_UNRESOLVED_FE = 0xfe,
    TURN_COMMAND_UNRESOLVED_FF = 0xff
} turn_command_kind_e;

/* A unit's command, kept in its map_unit_t. ACT carries the action of the AI's selected action
 * (ai_action_data_t.command); MOVE writes only the destination's x, layer and y, and END_TURN leaves
 * the payload as it was. */
typedef struct turn_command {
    s32 kind; /* TURN_COMMAND_*, or the current menu's targeting/crystal choice. */
    union {
        map_tile_coordinates_t move;
        action_command_t action;
    } data;
} turn_command_t;
typedef char turn_command_size_must_be_24[sizeof(turn_command_t) == 24 ? 1 : -1];

#endif

#ifndef TURN_COMMAND_H
#define TURN_COMMAND_H

/* The turn command: the next thing a unit does, built by the AI or taken from a player's menu choice. */

#include "fft/battle_map.h"
#include "fft/unit.h"
#include "psx/types.h"

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
    TURN_COMMAND_REFRESH_CONTROL = 18, /* copy the control flag back from the stats */
    TURN_COMMAND_MOVE_SELF = 25,       /* MOVE that never redirects to the unit's mount */
    TURN_COMMAND_UNRESOLVED_FE = 0xfe,
    TURN_COMMAND_UNRESOLVED_FF = 0xff
} turn_command_kind_e;

/* A unit's command, kept in its unit_runtime_t. ACT carries the action of the AI's selected action
 * (ai_action_data_t.command); MOVE writes only the destination's x, layer and y, and END_TURN leaves
 * the payload as it was. */
typedef struct turn_command {
    s32 kind; /* TURN_COMMAND_*, or the current menu's targeting/crystal choice. */
    union {
        battle_map_tile_coordinates_t move;
        action_command_t action;
    } data;
} turn_command_t;
typedef char turn_command_size_must_be_24[sizeof(turn_command_t) == 24 ? 1 : -1];

#endif

#ifndef BATTLE_AI_COMMAND_H
#define BATTLE_AI_COMMAND_H

/* The turn command: the next thing a unit does, built by the AI or taken from a player's menu choice. */

#include "fft/map.h"
#include "fft/unit.h"
#include "psx/types.h"

/* commands */
typedef enum ai_command_kind {
    AI_COMMAND_MOVE = 0,
    AI_COMMAND_ACT = 1,
    AI_COMMAND_END_TURN = 2,
    /* The rest reach the command from menu_resolve_selection, for a player's menu choice.
     * The auto-battle choices are 12 plus twice the option, plus one once a unit has been
     * picked for the two that name one (SELECT picks it, DISPLAY shows it); SELECT and the two
     * others equal their ai_autobattle_mode_e values. */
    AI_COMMAND_NO_MENU_RECORD = 7, /* the current menu id has no record: back to the turn events */
    AI_COMMAND_AUTOBATTLE_TARGETED_SELECT = 12,
    AI_COMMAND_AUTOBATTLE_TARGETED_DISPLAY = 13,
    AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT = 14,
    AI_COMMAND_AUTOBATTLE_RECOVERY_DISPLAY = 15,
    AI_COMMAND_AUTOBATTLE_FADING_LIFE = 16,
    AI_COMMAND_AUTOBATTLE_RETREAT = 17,
    AI_COMMAND_REFRESH_CONTROL = 18, /* copy the control flag back from the stats */
    AI_COMMAND_MOVE_SELF = 25,       /* MOVE that never redirects to the unit's mount */
    AI_COMMAND_UNRESOLVED_FE = 0xfe,
    AI_COMMAND_UNRESOLVED_FF = 0xff,
} ai_command_kind_e;

/* How an action follows its target: action_command_t.targeting_type, in the AI and menu commands
 * and in the command a unit stores. */
typedef enum battle_targeting_type {
    BATTLE_TARGETING_TYPE_TILE = 5, /* aims at target_position */
    BATTLE_TARGETING_TYPE_UNIT = 6, /* follows the unit target_id */
} battle_targeting_type_e;

/* A unit's command, kept in its battle_unit_t. ACT carries the action of the AI's selected action
 * (ai_action_data_t.command); MOVE writes only the destination's x, layer and y, and END_TURN leaves
 * the payload as it was. */
typedef struct ai_command {
    ai_command_kind_e kind;
    union {
        map_tile_coordinates_t move;
        action_command_t action;
    } data;
} ai_command_t;
typedef char ai_command_size_must_be_24[sizeof(ai_command_t) == 24 ? 1 : -1];

/* building a command */
/* ai_data_t.main_ai_state: the stage battle_ai_build_command resumes at after it ran out of frame
 * time. */
typedef enum ai_command_build_stage {
    AI_COMMAND_BUILD_REFRESH = 0,
    AI_COMMAND_BUILD_MOVEMENT = 1,
    AI_COMMAND_BUILD_CHOOSE_ACTION = 2,
} ai_command_build_stage_e;

extern ai_command_t g_battle_ai_command_history[2]; /* the last two commands built, newest last */

/* Returns -1 while the decision is still running (it resumes on the next call), else 0 with
 * command filled. */
s32 battle_ai_build_command(s32 unit_id, ai_command_t* command);

#endif

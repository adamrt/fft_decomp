#ifndef WLDCORE_SCRIPT_H
#define WLDCORE_SCRIPT_H

/* WLDCORE's world map script: the conditional blocks a location runs, and the map requests they stage. */

#include "psx/types.h"

/* results and requests */
/* location_script_state_t.flags. The interpreter clears them before each block. Comparisons set CONDITION_FAILED
 * when their predicate rejects the block; action opcodes set ACTION_REACHED to end the block and an action bit when
 * they stage a request. */
typedef enum location_script_result_flags {
    LOCATION_SCRIPT_RESULT_ACTION_REACHED = 0x001,
    LOCATION_SCRIPT_RESULT_CONDITION_FAILED = 0x002,
} location_script_result_flags_e;

typedef enum location_script_action_flags {
    LOCATION_SCRIPT_ACTION_CHOICE = 0x004,
    LOCATION_SCRIPT_ACTION_LOAD_EVENT = 0x008,
    LOCATION_SCRIPT_ACTION_TEXT = 0x010,
    LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY = 0x020,
    LOCATION_SCRIPT_ACTION_SET_VAR_2 = 0x040,
    LOCATION_SCRIPT_ACTION_DRAW_PATH = 0x080,
    LOCATION_SCRIPT_ACTION_ERASE_PATH = 0x100,
    LOCATION_SCRIPT_ACTION_FOCUS_LOCATION = 0x200,
    LOCATION_SCRIPT_ACTION_DRAW_LOCATION = 0x400,
    LOCATION_SCRIPT_ACTION_ERASE_LOCATION = 0x800,
    LOCATION_SCRIPT_ACTION_PATH_MASK = LOCATION_SCRIPT_ACTION_DRAW_PATH | LOCATION_SCRIPT_ACTION_ERASE_PATH,
    LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK
    = LOCATION_SCRIPT_ACTION_DRAW_LOCATION | LOCATION_SCRIPT_ACTION_ERASE_LOCATION,
    LOCATION_SCRIPT_ACTION_MAP_MENU_MASK = LOCATION_SCRIPT_ACTION_PATH_MASK | LOCATION_SCRIPT_ACTION_FOCUS_LOCATION
        | LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK,
} location_script_action_flags_e;

/* What an action opcode stores: ACTION_REACHED with its action bit. */
typedef enum location_script_request_flags {
    LOCATION_SCRIPT_REQUEST_CHOICE = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_CHOICE,
    LOCATION_SCRIPT_REQUEST_LOAD_EVENT = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_LOAD_EVENT,
    LOCATION_SCRIPT_REQUEST_TEXT = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_TEXT,
    LOCATION_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY,
    LOCATION_SCRIPT_REQUEST_SET_VAR_2 = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_SET_VAR_2,
    LOCATION_SCRIPT_REQUEST_DRAW_PATH = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DRAW_PATH,
    LOCATION_SCRIPT_REQUEST_ERASE_PATH = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_ERASE_PATH,
    LOCATION_SCRIPT_REQUEST_FOCUS_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_FOCUS_LOCATION,
    LOCATION_SCRIPT_REQUEST_DRAW_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DRAW_LOCATION,
    LOCATION_SCRIPT_REQUEST_ERASE_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_ERASE_LOCATION,
} location_script_request_flags_e;

/* interpreter */
/* The interpreter's state; the opcode handlers read ip and data here. It is one object through the choice and
 * argument values: location_script_opcode_1a_request_choice reaches ip at -4 and the arguments at +0x10 from its
 * &choice_vars base. */
typedef struct location_script_state {
    s32 flags;          /* 0x00; location_script_result_flags_e | location_script_action_flags_e */
    u16* list;          /* 0x04; script list for the current location */
    u16* data;          /* 0x08 */
    u16 ip;             /* 0x0c */
    s32 choice_vars[4]; /* 0x10; staged Choice variables */
    /* 0x20: script request arguments; the staged Choice values share them. */
    s32 args[4];
} location_script_state_t;
typedef char wldcore_script_state_size_must_be_48[sizeof(location_script_state_t) == 48 ? 1 : -1];

/* The loaded world map script: a 16-bit offset per location to its list of conditional blocks. */
extern u16* g_location_script_default_base;
extern u16* g_location_script_base;
extern location_script_state_t g_location_script_state;
/* Scalar view of g_location_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses.
 * The load_* opcode handlers declare volatile views of flags/data/ip locally. */
extern s32 g_location_script_flags;
extern void (*g_location_script_opcodes[])(void); /* by opcode */
s32 location_script_run(s32 location, s32 action_mask);
void location_script_opcode_19_request_load_event(void);
void location_script_opcode_1d_request_text(void);
void location_script_opcode_1e_request_deep_dungeon_entry(void);
void location_script_opcode_1f_request_set_var_2(void);
void location_script_opcode_20_request_draw_path(void);
void location_script_opcode_21_request_erase_path(void);

#endif

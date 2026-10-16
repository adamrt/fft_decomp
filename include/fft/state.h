#ifndef STATE_H
#define STATE_H

/* BATTLE's game state machine: the frame loop, the handler and entry function of each game state, the map jumps and the
 * battle's close, and the game options word. */

#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* game state */
/*
 * Values dispatched by state_run_game_loop, verified against the
 * target's switch table.  The target has no explicit handler for value 50.
 */
typedef enum game_state {
    GAME_STATE_FREE_CURSOR = 0,
    GAME_STATE_FREE_CURSOR_HELP = 1,
    GAME_STATE_HIGHLIGHT_UNITS = 2,
    GAME_STATE_OPEN_ACTION_MENUS = 3,
    GAME_STATE_IDLING_ACTION_MENUS = 4,
    GAME_STATE_AI_COMMAND = 5,
    GAME_STATE_ACTION_HELP_MENU = 6,
    GAME_STATE_DISPLAY_MOVE_AREA = 7,
    GAME_STATE_MINI_MENU = 8,
    GAME_STATE_MINI_MENU_HELP = 9,
    GAME_STATE_ACTIVE_TURN = 10,
    GAME_STATE_STATUS_EXECUTE = 11,
    GAME_STATE_UNIT_MOVE = 12,
    GAME_STATE_SELECT_MOVE_TILE = 13,
    GAME_STATE_MOVE_RANGE_EXCEPTION = 14,
    GAME_STATE_ILLEGAL_MOVE_MENU = 15,
    GAME_STATE_UNIT_MOVING_SETUP = 16,
    GAME_STATE_UNIT_MOVING = 17,
    GAME_STATE_MOVE_CONFIRM_MENU = 18,
    GAME_STATE_WAIT_DIRECTION = 19,
    GAME_STATE_WAIT_MENU = 20,
    GAME_STATE_CRYSTAL_LEARN = 21,
    GAME_STATE_TARGETING_PROMPT = 22,
    GAME_STATE_TARGETING_RANGE = 23,
    GAME_STATE_ILLEGAL_RANGE = 24,
    GAME_STATE_ABILITY_PREVIEW_HANDLING = 25,
    GAME_STATE_ABILITY_PREVIEW_HELP = 26,
    GAME_STATE_CONFIRM_ACTION = 27,
    GAME_STATE_PRE_ATTACK_ANIMATION = 28,
    GAME_STATE_ACTION_CAST = 29,
    GAME_STATE_COMMENCE_ATTACK_PHASE = 30,
    GAME_STATE_TARGET_SELECT_START = 31,
    GAME_STATE_TARGET_SELECT = 32,
    GAME_STATE_TARGET_SELECT_DENIED = 33,
    GAME_STATE_TARGET_SELECT_CONFIRM = 34,
    GAME_STATE_TARGET_DISPLAY_START = 35,
    GAME_STATE_TARGET_DISPLAY = 36,
    GAME_STATE_AFTER_COMMAND = 37,
    GAME_STATE_CONTINUE_TURN = 38,
    GAME_STATE_CHANGE_TURN = 39,
    GAME_STATE_LEARN_ABILITY_ON_HIT = 40,
    GAME_STATE_ACTION_ANNOUNCEMENT = 41,
    GAME_STATE_OPEN_SP2_FILES = 42,
    GAME_STATE_START_EFFECT_FILE_OPEN = 43,
    GAME_STATE_ACTION_EXECUTE_START = 44,
    GAME_STATE_ACTION_EXECUTE = 45,
    GAME_STATE_BATTLE_MESSAGE_DISPLAY = 46,
    GAME_STATE_RESUME_ATTACK_PHASE = 47,
    GAME_STATE_DEEP_DUNGEON_MESH_LOAD = 48,
    GAME_STATE_DEEP_DUNGEON_MESH_FINISH = 49,
    GAME_STATE_EFFECT = 51,
    GAME_STATE_EVENT = 52,
    GAME_STATE_MAP_JUMPING_OUT = 53,
    GAME_STATE_MAP_INITIALIZE = 54,
    GAME_STATE_MAP_JUMPING_IN = 55,
    GAME_STATE_MAP_JUMPING_OUT_2 = 56,
    GAME_STATE_MAP_INITIALIZE_2 = 57,
    GAME_STATE_MAP_JUMPING_IN_2 = 58,
    GAME_STATE_CLOSE_BATTLE = 59,
} game_state_e;

extern s32 g_game_state;
extern s32 g_game_state_previous;
extern s32 g_state_map_init_step;

/* What MAIN does once state_run_game_loop returns: a system_game_flow_e. */
extern u32 g_game_flow_state;

/* Nonzero while the action's effect is pending: 1 while its file loads, 2 while it plays, with
 * the knockback targets' step phases ORed in. SEQ ops 0xd3 and 0xd6 and
 * script_play_effect_and_wait wait on it. */
extern s32 g_state_animation_continue_check;

void state_run_game_loop(void);
s32 state_sync_frame(u32* ordering_table);
s32 state_update_controller_input(void);
void state_update_units(void);
s32 state_get_animation_continue_check(void);

/* state handlers */
/* The per-frame handler of each game_state_e, in state order. ACTION_EXECUTE and EVENT run
 * an update function instead. */
void state_handle_free_cursor(void);
void state_handle_free_cursor_help(void);
void state_handle_highlight_units(void);
void state_handle_open_action_menus(void);
void state_handle_idling_action_menus(void);
void state_handle_ai_command(void);
void state_handle_action_help_menu(void);
void state_handle_display_move_area(void);
void state_handle_mini_menu(void);
void state_handle_mini_menu_help(void);
void state_handle_active_turn(void);
void state_handle_status_execute(void);
void state_handle_unit_move(void);
void state_handle_select_move_tile(void);
void state_handle_move_range_exception(void);
void state_handle_illegal_move_menu(void);
void state_handle_unit_moving_setup(void);
void state_handle_unit_moving(void);
void state_handle_move_confirm_menu(void);
void state_handle_wait_direction(void);
void state_handle_wait_menu(void);
void state_handle_crystal_learn(void);
void state_handle_targeting_prompt(void);
void state_handle_targeting_range(void);
void state_handle_illegal_range(void);
void state_handle_ability_preview(void);
void state_handle_ability_preview_help(void);
void state_handle_confirm_action(void);
void state_handle_pre_attack_animation(void);
void state_handle_action_cast(void);
void state_handle_commence_attack_phase(void);
void state_handle_target_select_start(void);
void state_handle_target_select(void);
void state_handle_target_select_denied(void);
void state_handle_target_select_confirm(void);
void state_handle_target_display_start(void);
void state_handle_target_display(void);
void state_handle_after_command(void);
void state_handle_continue_turn(void);
void state_handle_change_turn(void);
void state_handle_learn_ability_on_hit(void);
void state_handle_action_announcement(void);
void state_handle_open_sp2_files(void);
void state_handle_start_effect_file_open(void);
void state_handle_action_execute_start(void);
void state_handle_action_execute(void);
void state_handle_battle_message_display(void);
void state_handle_resume_attack_phase(void);
void state_handle_deep_dungeon_mesh_load(void);
void state_handle_deep_dungeon_mesh_finish(void);
void state_handle_effect(void);
void state_handle_event(void);
void state_handle_map_jumping_out(void);
void state_handle_map_init(void);
void state_handle_map_jumping_in(void);
void state_handle_change_map_jumping_out(void);
void state_handle_change_map_init(void);
void state_handle_change_map_jumping_in(void);
void state_handle_close_battle(void);

/* entering a state */
extern s32 g_state_in_help;
extern s32 g_state_wait_direction_facing;
extern s32 g_state_window_closed;

/* Each sets up a state and switches g_game_state to it. */
void state_enter_free_cursor(void);
void state_enter_free_cursor_help(void);
void state_enter_highlight_units(void);
void state_enter_open_action_menus(void);
void state_enter_action_help_menu(void);
void state_enter_display_move_area(void);
void state_enter_mini_menu(void);
void state_return_to_mini_menu(void);
void state_enter_mini_menu_help(void);
void state_enter_status_execute(void);
void state_enter_unit_move(void);
void state_enter_select_move_tile(void);
void state_enter_illegal_move_menu(void);
void state_enter_unit_moving_setup(void);
void state_enter_unit_moving(void);
void state_enter_move_confirm_menu(void);
void state_enter_wait_direction(void);
void state_enter_wait_menu(void);
void state_enter_targeting_prompt(void);
void state_enter_illegal_range(void);
void state_enter_confirm_action(void);
void state_enter_pre_attack_animation(void);
void state_enter_action_cast(void);
void state_enter_commence_attack_phase(void);
void state_enter_target_select(void);
void state_enter_target_select_denied(void);
void state_enter_target_select_confirm(void);
void state_enter_target_display(void);
void state_enter_after_command(void);
void state_enter_continue_turn(void);
void state_enter_learn_ability_on_hit(void);
void state_enter_open_sp2_files(void);
void state_enter_action_execute(void);
void state_enter_battle_message_display(void);
void state_enter_event(void);
void state_enter_event_at_battle_start(void);

/* map jumps and closing the battle */
void state_start_map_jump_out(s32 map_id, s32 duration);
void state_start_change_map_jump_out(s32 map_id, s32 duration);
void state_start_change_map_jump_in(s32 duration);
void state_enter_close_battle_to_world(s32 duration);

/* Queues CLOSE_BATTLE; the callers always pass transition step 8. */
void state_enter_close_battle(s32 transition_step, s32 close_flow_state);

/* text lookup */
/* MAIN selects the BATTLE address of text_find_entry through this existing alias. */
u8* battle_text_find_entry(u32 text_id);

/* game options */
/* The live options word is g_game_options. The options screen unpacks its fields in menu order. Toggle
 * settings encode On as 0 and Off as 1; the paired high bit participates in the menu's Customize/Initialize
 * state. */
enum {
    GAME_OPTIONS_NAVIGATION_MESSAGES_MASK = 0x00018000,
    GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK = 0x60000000,
    GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS = 0x00008000,
};

typedef enum game_option_index {
    GAME_OPTION_CURSOR_MOVEMENT = 0,
    GAME_OPTION_CURSOR_REPEAT_SPEED = 1,
    GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED = 2,
    GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED = 3,
    GAME_OPTION_MESSAGE_DISPLAY_SPEED = 4,
    GAME_OPTION_NAVIGATION_MESSAGES = 5,
    GAME_OPTION_ABILITY_NAMES = 6,
    GAME_OPTION_EFFECT_MESSAGES = 7,
    GAME_OPTION_DISPLAY_GAINED_EXP_JP = 8,
    GAME_OPTION_TARGET_FLASHING = 9,
    GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS = 10,
    GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE = 11,
    GAME_OPTION_SOUND_MODE = 12,
    GAME_OPTION_COUNT = 13,
    GAME_OPTION_UNCHANGED = 13,
} game_option_index_e;

typedef enum game_option_toggle {
    GAME_OPTION_ON = 0,
    GAME_OPTION_OFF = 1,
} game_option_toggle_e;

typedef enum game_cursor_movement {
    GAME_CURSOR_MOVEMENT_TYPE_A = 0,
    GAME_CURSOR_MOVEMENT_TYPE_B = 1,
} game_cursor_movement_e;

typedef enum game_cursor_repeat_speed {
    GAME_CURSOR_REPEAT_SPEED_FAST = 0,
    GAME_CURSOR_REPEAT_SPEED_REGULAR = 1,
    GAME_CURSOR_REPEAT_SPEED_SLOW = 2,
} game_cursor_repeat_speed_e;

typedef enum game_multi_height_cursor_speed {
    GAME_MULTI_HEIGHT_CURSOR_SPEED_FAST = 0,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR = 1,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_SLOW = 2,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP = 3,
} game_multi_height_cursor_speed_e;

typedef enum game_finger_cursor_repeat_speed {
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTEST = 0,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTER = 1,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FAST = 2,
    GAME_FINGER_CURSOR_REPEAT_SPEED_REGULAR = 3,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOW = 4,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWER = 5,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWEST = 6,
} game_finger_cursor_repeat_speed_e;

typedef enum game_message_display_speed {
    GAME_MESSAGE_DISPLAY_SPEED_FAST = 0,
    GAME_MESSAGE_DISPLAY_SPEED_REGULAR = 1,
    GAME_MESSAGE_DISPLAY_SPEED_SLOW = 2,
} game_message_display_speed_e;

typedef struct game_options_fields {
    u32 cursor_movement : 3;
    u32 cursor_repeat_speed : 3;
    u32 multi_height_cursor_speed : 3;
    u32 finger_cursor_repeat_speed : 3;
    u32 message_display_speed : 3;
    u32 navigation_messages : 2;
    u32 ability_names : 2;
    u32 effect_messages : 2;
    u32 sound_mode : 2; /* suzuki_sound_type_e */
    u32 show_unequippable_items : 2;
    u32 display_gained_exp_jp : 2;
    u32 target_flashing : 2;
    u32 max_equip_at_job_change : 2;
    u32 _unused_03_bit7 : 1;
} game_options_fields_t;

/* The reset loads and stores this four-byte record as a scalar; a struct copy
 * moves the defaults load ahead of the input-timing stores. */
typedef union game_options {
    u32 value;
    game_options_fields_t fields;
} game_options_t;
typedef char game_options_size_must_be_4[sizeof(game_options_t) == 4 ? 1 : -1];

extern game_options_t g_game_options;
extern game_options_t g_game_options_defaults;

/* The player's options, saved while a tutorial event forces its own and restored when it ends. BATTLE and WORLD each
 * define it. */
extern game_options_t g_game_options_saved;

#endif

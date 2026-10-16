#ifndef BATTLE_H
#define BATTLE_H

/* BATTLE.BIN, the battle engine: its game state machine and frame pacing, deployment, the unit
 * table, the ability animation tables, the free cursor, its heap and small helpers several parts
 * share. Event and effect overlays run alongside it. */

#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* game state */
/*
 * Values dispatched by battle_state_run_game_loop, verified against the
 * target's switch table.  The target has no explicit handler for value 50.
 */
typedef enum battle_game_state {
    BATTLE_GAME_STATE_FREE_CURSOR = 0,
    BATTLE_GAME_STATE_FREE_CURSOR_HELP = 1,
    BATTLE_GAME_STATE_HIGHLIGHT_UNITS = 2,
    BATTLE_GAME_STATE_OPEN_ACTION_MENUS = 3,
    BATTLE_GAME_STATE_IDLING_ACTION_MENUS = 4,
    BATTLE_GAME_STATE_AI_COMMAND = 5,
    BATTLE_GAME_STATE_ACTION_HELP_MENU = 6,
    BATTLE_GAME_STATE_DISPLAY_MOVE_AREA = 7,
    BATTLE_GAME_STATE_MINI_MENU = 8,
    BATTLE_GAME_STATE_MINI_MENU_HELP = 9,
    BATTLE_GAME_STATE_ACTIVE_TURN = 10,
    BATTLE_GAME_STATE_STATUS_EXECUTE = 11,
    BATTLE_GAME_STATE_UNIT_MOVE = 12,
    BATTLE_GAME_STATE_SELECT_MOVE_TILE = 13,
    BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION = 14,
    BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU = 15,
    BATTLE_GAME_STATE_UNIT_MOVING_SETUP = 16,
    BATTLE_GAME_STATE_UNIT_MOVING = 17,
    BATTLE_GAME_STATE_MOVE_CONFIRM_MENU = 18,
    BATTLE_GAME_STATE_WAIT_DIRECTION = 19,
    BATTLE_GAME_STATE_WAIT_MENU = 20,
    BATTLE_GAME_STATE_CRYSTAL_LEARN = 21,
    BATTLE_GAME_STATE_TARGETING_PROMPT = 22,
    BATTLE_GAME_STATE_TARGETING_RANGE = 23,
    BATTLE_GAME_STATE_ILLEGAL_RANGE = 24,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING = 25,
    BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP = 26,
    BATTLE_GAME_STATE_CONFIRM_ACTION = 27,
    BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION = 28,
    BATTLE_GAME_STATE_ACTION_CAST = 29,
    BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE = 30,
    BATTLE_GAME_STATE_TARGET_SELECT_START = 31,
    BATTLE_GAME_STATE_TARGET_SELECT = 32,
    BATTLE_GAME_STATE_TARGET_SELECT_DENIED = 33,
    BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM = 34,
    BATTLE_GAME_STATE_TARGET_DISPLAY_START = 35,
    BATTLE_GAME_STATE_TARGET_DISPLAY = 36,
    BATTLE_GAME_STATE_AFTER_COMMAND = 37,
    BATTLE_GAME_STATE_CONTINUE_TURN = 38,
    BATTLE_GAME_STATE_CHANGE_TURN = 39,
    BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT = 40,
    BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT = 41,
    BATTLE_GAME_STATE_OPEN_SP2_FILES = 42,
    BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN = 43,
    BATTLE_GAME_STATE_ACTION_EXECUTE_START = 44,
    BATTLE_GAME_STATE_ACTION_EXECUTE = 45,
    BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY = 46,
    BATTLE_GAME_STATE_RESUME_ATTACK_PHASE = 47,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD = 48,
    BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH = 49,
    BATTLE_GAME_STATE_EFFECT = 51,
    BATTLE_GAME_STATE_EVENT = 52,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT = 53,
    BATTLE_GAME_STATE_MAP_INITIALIZE = 54,
    BATTLE_GAME_STATE_MAP_JUMPING_IN = 55,
    BATTLE_GAME_STATE_MAP_JUMPING_OUT_2 = 56,
    BATTLE_GAME_STATE_MAP_INITIALIZE_2 = 57,
    BATTLE_GAME_STATE_MAP_JUMPING_IN_2 = 58,
    BATTLE_GAME_STATE_CLOSE_BATTLE = 59,
} battle_game_state_e;

extern s32 g_battle_game_state;
extern s32 g_battle_previous_game_state;
/* What MAIN does once battle_state_run_game_loop returns: a main_game_flow_state_e. */
extern u32 g_battle_game_flow_state;
/* Nonzero while the action's effect is pending: 1 while its file loads, 2 while it plays, with
 * the knockback targets' step phases ORed in. SEQ ops 0xd3 and 0xd6 and
 * script_play_effect_and_wait wait on it. */
extern s32 g_battle_state_animation_continue_check;
extern s32 g_battle_state_map_init_step;
extern s32 g_battle_state_screen_fade_step;

void battle_state_run_game_loop(void);
s32 battle_state_get_animation_continue_check(void);
void battle_state_stop_map_animations(void);
s32 battle_state_sync_frame(u32* ordering_table);
s32 battle_state_try_enter_scenario_event(battle_unit_t* unit);
s32 battle_state_update_controller_input(void);
void battle_state_update_units(void);

/* state handlers */
/* The per-frame handler of each battle_game_state_e, in state order. ACTION_EXECUTE and EVENT run
 * an update function instead. */
void battle_state_handle_free_cursor_state(void);
void battle_state_handle_free_cursor_help_state(void);
void battle_state_handle_highlight_units_state(void);
void battle_state_handle_open_action_menus_state(void);
void battle_state_handle_idling_action_menus_state(void);
void battle_state_handle_ai_command_state(void);
void battle_state_handle_action_help_menu_state(void);
void battle_state_handle_display_move_area_state(void);
void battle_state_handle_mini_menu_state(void);
void battle_state_handle_mini_menu_help_state(void);
void battle_state_handle_active_turn_state(void);
void battle_state_handle_status_execute_state(void);
void battle_state_handle_unit_move_state(void);
void battle_state_handle_select_move_tile_state(void);
void battle_state_handle_move_range_exception_state(void);
void battle_state_handle_illegal_move_menu_state(void);
void battle_state_handle_unit_moving_setup_state(void);
void battle_state_handle_unit_moving_state(void);
void battle_state_handle_move_confirm_menu_state(void);
void battle_state_handle_wait_direction_state(void);
void battle_state_handle_wait_menu_state(void);
void battle_state_handle_crystal_learn_state(void);
void battle_state_handle_targeting_prompt_state(void);
void battle_state_handle_targeting_range_state(void);
void battle_state_handle_illegal_range_state(void);
void battle_state_handle_ability_preview_state(void);
void battle_state_handle_ability_preview_help_state(void);
void battle_state_handle_confirm_action_state(void);
void battle_state_handle_pre_attack_animation_state(void);
void battle_state_handle_action_cast_state(void);
void battle_state_handle_commence_attack_phase_state(void);
void battle_state_handle_target_select_start_state(void);
void battle_state_handle_target_select_state(void);
void battle_state_handle_target_select_denied_state(void);
void battle_state_handle_target_select_confirm_state(void);
void battle_state_handle_target_display_start_state(void);
void battle_state_handle_target_display_state(void);
void battle_state_handle_after_command_state(void);
void battle_state_handle_continue_turn_state(void);
void battle_state_handle_change_turn_state(void);
void battle_state_handle_learn_ability_on_hit_state(void);
void battle_state_handle_action_announcement_state(void);
void battle_state_handle_open_sp2_files_state(void);
void battle_state_handle_start_effect_file_open_state(void);
void battle_state_handle_action_execute_start_state(void);
void battle_state_update_action_execute_mode(void);
void battle_state_handle_battle_message_display_state(void);
void battle_state_handle_resume_attack_phase_state(void);
void battle_state_handle_deep_dungeon_mesh_load_state(void);
void battle_state_handle_deep_dungeon_mesh_finish_state(void);
void battle_state_handle_effect_state(void);
void battle_state_update_event_mode(void);
void battle_state_handle_map_jumping_out_state(void);
void battle_state_handle_map_init_state(void);
void battle_state_handle_map_jumping_in_state(void);
void battle_state_handle_change_map_jumping_out_state(void);
void battle_state_handle_change_map_init_state(void);
void battle_state_handle_change_map_jumping_in_state(void);
void battle_state_handle_close_battle_state(void);

/* state transitions */
/* Each sets up a state and switches g_battle_game_state to it; battle_state_announce_next_charged_action
 * only when a charged action is due. */
s32 battle_state_announce_next_charged_action(void);
void battle_state_enter_action_cast(void);
void battle_state_enter_action_execute(void);
void battle_state_enter_targeting_prompt(void);
void battle_state_enter_after_command(void);
void battle_state_enter_commence_attack_phase(void);
void battle_state_enter_continue_turn(void);
void battle_state_enter_event(void);
void battle_state_enter_highlight_units_by_team(void);
void battle_state_enter_open_sp2_files(void);
void battle_state_enter_pre_attack_animation(void);
void battle_state_enter_select_move_tile(void);
void battle_state_enter_status_execute(void);
void battle_state_enter_target_display(void);
void battle_state_enter_target_out_of_range(void);
void battle_state_enter_target_select(void);
void battle_state_enter_target_select_confirm(void);
void battle_state_enter_target_select_denied(void);
void battle_state_enter_unit_moving(void);
void battle_state_enter_unit_moving_setup(void);
void battle_state_set_free_cursor(void);
void battle_state_start_battle_message_display(void);
void battle_state_start_change_map_jump_in(s32 duration);
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_close_battle(s32 duration);
void battle_state_start_map_jump_out(s32 map_id, s32 duration);
/* Queues CLOSE_BATTLE; the callers always pass transition step 8. */
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state);

/* frame pacing */
void battle_state_clear_frame_pacing(void);
s32 battle_state_get_vsync_interval(void);
void battle_state_reset_frame_pacing_timer(void);
s32 battle_state_set_animation_speed(s32 speed);
void battle_state_set_min_vsync_interval(s32 value);

/* deployment */
extern palette_t g_battle_deployed_unit_palettes[2];
extern const RECT g_battle_deployment_clear_rect;
extern s32 g_battle_deployment_skipped;
/* The four random-battle music tracks (0x04, 0x05, 0x0e, 0x0f); map initialization plays the
 * one at rand() / 0x2000. */
extern u8 g_battle_random_battle_music_tracks[];

void battle_deployment_mark_finished(void);
void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 battle_state_sync_and_submit_deployment_frame(u32* ordering_table);
s32 battle_state_update_deployment_controller_input(void);

/* units */
/* The live units, indexed by runtime id. */
extern battle_unit_t g_battle_units[BATTLE_UNIT_RUNTIME_SLOT_COUNT];

/* ability playback */
extern s16 g_battle_ability_effect_id;
extern u8 g_battle_loaded_ability_flags_1;
extern battle_ability_animation_data_t g_battle_ability_animation_data[BATTLE_ABILITY_ANIMATION_COUNT];
/* Two battle_unit_animation_e for each set: the one that starts a charge and
 * the one held while the unit charges or performs; 0 for none. */
extern u8 g_battle_ability_charge_animation_sets[BATTLE_CHARGE_ANIMATION_SET_COUNT]
                                                [BATTLE_CHARGE_ANIMATION_VARIANT_COUNT];
/* battle_unit_animation_e of each item type's attack at a higher, a level and a lower target. */
extern u8 g_battle_weapon_attack_animations[][3];

s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id);

/* free cursor */
extern u32 g_battle_cursor_repeat_counter; /* free-cursor repeat counter */
extern u16 g_battle_cursor_screen_x;
extern s16 g_battle_cursor_screen_y;
extern s32 g_battle_cursor_x;
extern s32 g_battle_cursor_y;
extern s32 g_battle_cursor_z;

/* heap */
/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_battle_heap_rover) or on one of the 16
 * per-owner lists in g_battle_heap_owner_lists. */
typedef struct battle_heap_node {
    struct battle_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} battle_heap_node_t;

typedef struct battle_heap_owner_list {
    battle_heap_node_t* head;
    s32 _unused_04;
} battle_heap_owner_list_t;

extern battle_heap_node_t* g_battle_heap_base;
extern s16 g_battle_heap_block_count;
extern s32 g_battle_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */
extern battle_heap_owner_list_t g_battle_heap_owner_lists[];
extern battle_heap_node_t* g_battle_heap_rover;
/* The formats battle_heap_print_stats prints. */
extern const char g_battle_text_heap_dp_format[];     /* "DP %x\n" */
extern const char g_battle_text_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_battle_text_heap_task_format[];   /* "TASK %d %x\n" */
extern const char g_battle_text_min_heap_format[];    /* "MIN HEAP %x " */

void* battle_heap_alloc_block(u32 size, s32 kind);
void battle_heap_free_block(void* ptr);
void battle_heap_init(battle_heap_node_t* buffer, u32 buffer_size);
void battle_heap_print_stats(void);

/* text */
extern s32 g_battle_text_awaiting_input;

/* MAIN selects the BATTLE address of text_find_entry through this existing alias. */
u8* battle_text_find_entry(u32 text_id);

/* arithmetic and copies */
/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* start + (end - start) * factor / 256, the division an arithmetic shift. */
s32 battle_lerp_q8(s32 start, s32 end, s32 factor);

/* Copy forward using a signed destination-end address; nonpositive counts do nothing. */
void battle_mem_copy_bytes(void* destination, const void* source, s32 byte_count);
/* Copy byte_count / 2 unsigned 16-bit values; leave an odd final byte unchanged. */
void battle_mem_copy_u16_values(void* destination, const void* source, s32 byte_count);
/* Clear byte_count / 4 signed 32-bit values; leave any trailing bytes unchanged. */
void battle_mem_zero_32bit_values(void* destination, s32 byte_count);

/* shared state */
extern u32 g_battle_script_saved_event_input; /* the event input held while a script runs */

/* The unit-ready test script_wait_for_unit_ready polls: always 1, so the wait never yields. */
s32 battle_return_one(s32 unused_runtime_id);

/* unknown state */
extern s32 g_battle_map_timed_transform_unread;
/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_battle_gfx_unread_1;
/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_battle_gfx_unread_2;
/* Provisional: always 0; copied into the move config byte 0x1a, which nothing reads. */
extern u8 g_battle_move_config_unknown_1a_value;
/* Provisional: written only by the uncalled battle_action_store_acting_unit_value; never read. */
extern u8 g_battle_action_acting_unit_value;
/* Cleared with the effect camera modes; nothing reads it. */
extern s32 g_battle_effect_camera_unread_1;
/* Cleared with the effect camera modes; nothing reads it. */
extern s32 g_battle_effect_camera_unread_2;
/* Provisional: 0x80000000 when an arrow arc ends without a hit; nothing reads it. */
extern s32 g_battle_effect_arrow_miss_marker;
/* Provisional: never written; battle_heap_print_stats prints it x4 in its "DP" line. */
extern s32 g_battle_heap_debug_dp_words;
/* Cleared when the effect work-record chain is built; nothing reads it. */
extern s32 g_battle_effect_work_records_unread;
/* Cleared when an ability effect is requested; nothing reads it. */
extern s32 g_battle_effect_request_unread;
/* Provisional: never written; added to the "DP" figure of battle_heap_print_stats. */
extern s32 g_battle_heap_debug_dp_bytes;

#endif

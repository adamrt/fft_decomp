#ifndef BATTLE_H
#define BATTLE_H

/* BATTLE.BIN: the battle engine. Event and effect overlays run alongside it. */

#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* engine state */
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
    BATTLE_GAME_STATE_MENU_TO_TARGETING = 5,
    BATTLE_GAME_STATE_ACTION_HELP_MENU = 6,
    BATTLE_GAME_STATE_DISPLAY_MOVE_AREA = 7,
    BATTLE_GAME_STATE_MINI_MENU = 8,
    BATTLE_GAME_STATE_MINI_MENU_HELP = 9,
    BATTLE_GAME_STATE_ACTIVE_TURN = 10,
    BATTLE_GAME_STATE_STATUS_EXECUTE = 11,
    BATTLE_GAME_STATE_UNIT_MOVE = 12,
    BATTLE_GAME_STATE_CLOSE_MOVE_HELP = 13,
    BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION = 14,
    BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU = 15,
    BATTLE_GAME_STATE_UNIT_MOVING_SETUP = 16,
    BATTLE_GAME_STATE_UNIT_MOVING = 17,
    BATTLE_GAME_STATE_MOVE_CONFIRM_MENU = 18,
    BATTLE_GAME_STATE_WAIT_DIRECTION = 19,
    BATTLE_GAME_STATE_WAIT_MENU = 20,
    BATTLE_GAME_STATE_CRYSTAL_LEARN = 21,
    BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP = 22,
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
    BATTLE_GAME_STATE_JP_EXP_GAIN = 38,
    BATTLE_GAME_STATE_CHANGE_TURN = 39,
    BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT = 40,
    BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT = 41,
    BATTLE_GAME_STATE_OPEN_SP2_FILES = 42,
    BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN = 43,
    BATTLE_GAME_STATE_SECONDARY_EFFECT = 44,
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
/* Nonzero while the action's effect is pending: 1 while its file loads, 2 while it plays, with
 * the knockback targets' step phases ORed in. SEQ ops 0xd3 and 0xd6 and
 * script_play_effect_and_wait wait on it. */
extern s32 g_battle_state_animation_continue_check;
extern s32 g_battle_state_map_init_step;
extern s32 g_battle_state_screen_fade_step;

s32 battle_state_announce_next_charged_action(void);
void battle_state_enter_action_cast(void);
void battle_state_enter_action_execution_setup(void);
void battle_state_enter_after_command(void);
void battle_state_enter_commence_attack_phase(void);
void battle_state_enter_highlight_units_by_team(void);
void battle_state_enter_continue_turn(void);
void battle_state_enter_open_sp2_files(void);
void battle_state_enter_pre_attack_animation(void);
void battle_state_enter_status_execute(void);
void battle_state_enter_target_display(void);
void battle_state_enter_target_out_of_range(void);
void battle_state_enter_target_select(void);
void battle_state_enter_target_select_confirm(void);
void battle_state_enter_target_select_denied(void);
void battle_state_enter_unit_moving(void);
void battle_state_enter_unit_moving_setup(void);
s32 battle_state_get_animation_continue_check(void);
void battle_state_handle_ability_preview_help_state(void);
void battle_state_handle_ability_preview_state(void);
void battle_state_handle_action_announcement_state(void);
void battle_state_handle_action_cast_state(void);
void battle_state_handle_action_execute_setup_state(void);
void battle_state_handle_action_help_menu_state(void);
void battle_state_handle_active_turn_state(void);
void battle_state_handle_after_command_state(void);
void battle_state_handle_battle_message_display_state(void);
void battle_state_handle_change_map_init_state(void);
void battle_state_handle_change_map_jumping_in_state(void);
void battle_state_handle_change_map_jumping_out_state(void);
void battle_state_handle_change_turn_state(void);
void battle_state_handle_close_battle_state(void);
void battle_state_handle_close_move_help_state(void);
void battle_state_handle_commence_attack_phase_state(void);
void battle_state_handle_confirm_action_state(void);
void battle_state_handle_crystal_learn_state(void);
void battle_state_handle_deep_dungeon_mesh_finish_state(void);
void battle_state_handle_deep_dungeon_mesh_load_state(void);
/* Per-frame handlers dispatched by battle_state_run_game_loop. */
void battle_state_handle_free_cursor_state(void);
void battle_state_handle_display_move_area_state(void);
void battle_state_handle_effect_state(void);
void battle_state_handle_highlight_units_state(void);
void battle_state_handle_idling_action_menus_state(void);
void battle_state_handle_illegal_move_menu_state(void);
void battle_state_handle_illegal_range_state(void);
void battle_state_handle_jp_exp_gain_state(void);
void battle_state_handle_learn_ability_on_hit_state(void);
void battle_state_handle_map_init_state(void);
void battle_state_handle_map_jumping_in_state(void);
void battle_state_handle_map_jumping_out_state(void);
void battle_state_handle_menu_to_targeting_state(void);
void battle_state_handle_mini_menu_help_state(void);
void battle_state_handle_mini_menu_state(void);
void battle_state_handle_move_confirm_menu_state(void);
void battle_state_handle_move_range_exception_state(void);
void battle_state_handle_open_action_menus_state(void);
void battle_state_handle_open_sp2_files_state(void);
void battle_state_handle_pre_attack_animation_state(void);
void battle_state_handle_resume_attack_phase_state(void);
void battle_state_handle_secondary_effect_state(void);
void battle_state_handle_start_effect_file_open_state(void);
void battle_state_handle_status_execute_state(void);
void battle_state_handle_target_display_start_state(void);
void battle_state_handle_target_display_state(void);
void battle_state_handle_target_select_confirm_state(void);
void battle_state_handle_target_select_denied_state(void);
void battle_state_handle_target_select_start_state(void);
void battle_state_handle_target_select_state(void);
void battle_state_handle_targeting_range_state(void);
void battle_state_handle_unit_move_state(void);
void battle_state_handle_unit_moving_setup_state(void);
void battle_state_handle_unit_moving_state(void);
void battle_state_handle_wait_direction_state(void);
void battle_state_handle_wait_menu_state(void);
void battle_state_run_game_loop(void);
void battle_state_start_battle_message_display(void);
void battle_state_start_change_map_jump_in(s32 duration);
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_close_battle(s32 duration);
void battle_state_start_map_jump_out(s32 map_id, s32 duration);
void battle_state_stop_map_animations(void);
s32 battle_state_sync_frame(u32* ordering_table);
s32 battle_state_try_enter_scenario_event(battle_unit_t* unit);
void battle_state_update_action_execute_mode(void);
s32 battle_state_update_controller_input(void);
void battle_state_update_event_mode(void);

/* frame timing */
extern s32 g_main_system_min_vsync_interval;
extern s32 g_main_frame_pacing_suppressed;
extern s32 g_main_frame_pacing_timer;

void battle_state_clear_frame_pacing(void);
s32 battle_state_get_vsync_interval(void);
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state);
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

void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 battle_state_sync_and_submit_deployment_frame(u32* ordering_table);
s32 battle_state_update_deployment_controller_input(void);

/* ability playback */
extern s16 g_battle_ability_effect_id;
extern u8 g_battle_loaded_ability_flags_1;

s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id);

/* cursor */
extern s32 g_battle_cursor_previous_x;     /* cursor X before this step */
extern s32 g_battle_cursor_previous_y;     /* cursor Y before this step */
extern s32 g_battle_cursor_previous_z;     /* cursor level before this step */
extern u32 g_battle_cursor_repeat_counter; /* free-cursor repeat counter */
extern u16 g_battle_cursor_screen_x;
extern s16 g_battle_cursor_screen_y;
extern s32 g_battle_cursor_x;
extern s32 g_battle_cursor_y;
extern s32 g_battle_cursor_z;

void battle_state_handle_free_cursor_help_state(void);
void battle_state_update_units(void);
void battle_state_set_free_cursor(void);

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
extern u32 g_main_battle_heap_end_address;
extern s32 g_battle_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */
extern battle_heap_owner_list_t g_battle_heap_owner_lists[];
extern battle_heap_node_t* g_battle_heap_rover;

void* battle_heap_alloc_block(u32 size, s32 kind);
void battle_heap_free_block(void* ptr);
void battle_heap_init(battle_heap_node_t* buffer, u32 buffer_size);
void battle_heap_print_stats(void);

/* arithmetic and copies */
/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* shared state */
extern u8 g_battle_job_level_gained_flag;
/* Unit coordinates shared by camera tile tests and dialogue positioning. */
extern SVECTOR g_unit_scratch_coords;

/* stubs */
s32 battle_return_one(s32 unused_runtime_id);

/* unnamed */
extern s32 g_battle_unknown_800f668c;
/* Write-only: zeroed with the map render state and never read anywhere on
 * the disc. The 150 x 14-byte shape suggests per-instruction parameter
 * storage (an event opcode takes at most 14 parameters), but no code proves
 * it, so the names stay provisional. */
extern u8 g_battle_unknown_800f70b4[150][14];
extern u8 g_battle_unknown_800f78e8[150];
extern s16 g_battle_unknown_800b628c;
extern s16 g_battle_unknown_800b6290;
extern u8 g_battle_unknown_8018f4fc;
extern u8 g_battle_unknown_8019387c;
extern s32 g_battle_unknown_801b8b18;
extern s32 g_battle_unknown_801b8b1c;
extern s32 g_battle_unknown_801b8b98;
extern s32 g_battle_unknown_801bacc4;
extern s32 g_battle_unknown_801bc0c4;
extern s32 g_battle_unknown_801bc0d4;
extern s32 g_battle_unknown_801bc0d8;

#endif

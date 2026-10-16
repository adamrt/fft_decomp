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
/* What MAIN does once state_run_game_loop returns: a system_game_flow_e. */
extern u32 g_game_flow_state;
/* Nonzero while the action's effect is pending: 1 while its file loads, 2 while it plays, with
 * the knockback targets' step phases ORed in. SEQ ops 0xd3 and 0xd6 and
 * script_play_effect_and_wait wait on it. */
extern s32 g_state_animation_continue_check;
extern s32 g_state_map_init_step;
extern s32 g_fade_step;

void state_run_game_loop(void);
s32 state_get_animation_continue_check(void);
void battle_map_stop_animations(void);
s32 state_sync_frame(u32* ordering_table);
s32 event_try_enter_scenario(unit_t* unit);
s32 state_update_controller_input(void);
void state_update_units(void);

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

/* state transitions */
/* Each sets up a state and switches g_game_state to it; action_announce_next
 * only when a Hamedo, action or reaction is due. */
s32 action_announce_next(void);
void state_enter_action_cast(void);
void state_enter_action_execute(void);
void state_enter_targeting_prompt(void);
void state_enter_after_command(void);
void state_enter_commence_attack_phase(void);
void state_enter_continue_turn(void);
void state_enter_event(void);
void state_enter_highlight_units(void);
void state_enter_open_sp2_files(void);
void state_enter_pre_attack_animation(void);
void state_enter_select_move_tile(void);
void state_enter_status_execute(void);
void state_enter_target_display(void);
void state_enter_illegal_range(void);
void state_enter_target_select(void);
void state_enter_target_select_confirm(void);
void state_enter_target_select_denied(void);
void state_enter_unit_moving(void);
void state_enter_unit_moving_setup(void);
void state_enter_free_cursor(void);
void state_enter_battle_message_display(void);
void state_start_change_map_jump_in(s32 duration);
void state_start_change_map_jump_out(s32 map_id, s32 duration);
void state_enter_close_battle_to_world(s32 duration);
void state_start_map_jump_out(s32 map_id, s32 duration);
/* Queues CLOSE_BATTLE; the callers always pass transition step 8. */
void state_enter_close_battle(s32 transition_step, s32 close_flow_state);

/* frame pacing */
void system_clear_min_vsync_interval(void);
s32 system_get_vsync_interval(void);
void system_reset_frame_pacing_timer(void);
s32 system_set_vsync_interval(s32 speed);
void system_set_min_vsync_interval(s32 value);

/* deployment */
extern palette_t g_cursor_tile_glow_palettes[2];
extern const RECT g_deploy_clear_rect;
extern s32 g_deploy_skipped;
/* The four random-battle music tracks (0x04, 0x05, 0x0e, 0x0f); map initialization plays the
 * one at rand() / 0x2000. */
extern u8 g_battle_map_random_battle_music_tracks[];

void deploy_mark_finished(void);
void deploy_init_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 deploy_sync_and_submit_frame(u32* ordering_table);
s32 deploy_update_controller_input(void);

/* units */
/* The live units, indexed by runtime id. */
extern unit_t g_units[UNIT_RUNTIME_SLOT_COUNT];

/* ability playback */
extern s16 g_effect_id;
extern u8 g_action_loaded_ability_flags_1;
extern animation_ability_data_t g_ability_animation_data[ANIMATION_ABILITY_COUNT];
/* Two animation_e for each set: the one that starts a charge and
 * the one held while the unit charges or performs; 0 for none. */
extern u8 g_animation_ability_charge_sets[ANIMATION_CHARGE_SET_COUNT][ANIMATION_CHARGE_VARIANT_COUNT];
/* animation_e of each item type's attack at a higher, a level and a lower target. */
extern u8 g_animation_weapon_attacks[][3];

s32 ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id);

/* free cursor */
extern u32 g_cursor_repeat_counter; /* free-cursor repeat counter */
extern u16 g_cursor_screen_x;
extern s16 g_cursor_screen_y;
extern s32 g_cursor_x;
extern s32 g_cursor_y;
extern s32 g_cursor_z;

/* heap */
/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_effect_heap_rover) or on one of the 16
 * per-owner lists in g_effect_heap_owner_lists. */
typedef struct effect_heap_node {
    struct effect_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} effect_heap_node_t;

typedef struct effect_heap_owner_list {
    effect_heap_node_t* head;
    s32 _unused_04;
} effect_heap_owner_list_t;

extern effect_heap_node_t* g_effect_heap_base;
extern s16 g_effect_heap_block_count;
extern s32 g_effect_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */
extern effect_heap_owner_list_t g_effect_heap_owner_lists[];
extern effect_heap_node_t* g_effect_heap_rover;
/* The formats effect_heap_print_stats prints. */
extern const char g_effect_heap_dp_format[];     /* "DP %x\n" */
extern const char g_effect_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_effect_heap_task_format[];   /* "TASK %d %x\n" */
extern const char g_effect_heap_min_format[];    /* "MIN HEAP %x " */

void* effect_heap_alloc_block(u32 size, s32 kind);
void effect_heap_free_block(void* ptr);
void effect_heap_init(effect_heap_node_t* buffer, u32 buffer_size);
void effect_heap_print_stats(void);

/* text */
extern s32 g_text_awaiting_input;

/* MAIN selects the BATTLE address of text_find_entry through this existing alias. */
u8* battle_text_find_entry(u32 text_id);

/* arithmetic and copies */
/* A random value between `min` and `max`, in either order. Spelled with rand()
 * first, unlike RAND_BETWEEN; both spellings occur in the target. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* start + (end - start) * factor / 256, the division an arithmetic shift. */
s32 effect_lerp_q8(s32 start, s32 end, s32 factor);

/* Copy forward using a signed destination-end address; nonpositive counts do nothing. */
void ai_mem_copy_bytes(void* destination, const void* source, s32 byte_count);
/* Copy byte_count / 2 unsigned 16-bit values; leave an odd final byte unchanged. */
void ai_mem_copy_u16_values(void* destination, const void* source, s32 byte_count);
/* Clear byte_count / 4 signed 32-bit values; leave any trailing bytes unchanged. */
void ai_mem_zero_32bit_values(void* destination, s32 byte_count);

/* shared state */
extern u32 g_event_saved_input; /* the event input held while a script runs */

/* The unit-ready test script_wait_for_unit_ready polls: always 1, so the wait never yields. */
s32 unit_is_ready(s32 unused_runtime_id);

/* unknown state */
extern s32 g_weather_timed_transform_unread;
/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_sprite_unread_1;
/* Cleared with the sprite counters at render init; nothing reads it. */
extern s16 g_sprite_unread_2;
/* Provisional: always 0; copied into the move config byte 0x1a, which nothing reads. */
extern u8 g_move_config_unknown_1a_value;
/* Provisional: written only by the uncalled action_store_acting_unit_value; never read. */
extern u8 g_action_acting_unit_value;
/* Cleared with the effect camera modes; nothing reads it. */
extern s32 g_effect_camera_unread_1;
/* Cleared with the effect camera modes; nothing reads it. */
extern s32 g_effect_camera_unread_2;
/* Provisional: 0x80000000 when an arrow arc ends without a hit; nothing reads it. */
extern s32 g_effect_arrow_miss_marker;
/* Provisional: never written; effect_heap_print_stats prints it x4 in its "DP" line. */
extern s32 g_effect_heap_debug_dp_words;
/* Cleared when the effect work-record chain is built; nothing reads it. */
extern s32 g_effect_work_records_unread;
/* Cleared when an ability effect is requested; nothing reads it. */
extern s32 g_effect_request_unread;
/* Provisional: never written; added to the "DP" figure of effect_heap_print_stats. */
extern s32 g_effect_heap_debug_dp_bytes;

#endif

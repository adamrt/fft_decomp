#ifndef MAIN_H
#define MAIN_H

/* SCUS_942.21: the resident game loop, frame timing, errors, input, options and scenario state. */

#include "fft/menu.h"
#include "fft/menu_option.h"
#include "fft/script_instruction.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* game loop */
/* Bits of g_system_flags, the 32-bit control flags of the WLDCORE world-map
 * loop: frame state, what the frame leaves out, requests left by WORLD and
 * OPENING, and the reason the loop ends. */
typedef enum system_flags {
    /* world_map_entrypoint runs frames while set; world_map_init_state sets it, a level clears it to leave. */
    SYSTEM_FLAG_RUNNING = 0x00000001,
    /* Draw this frame with the 3D map; world_map_draw_frame_with_map clears it after drawing. */
    SYSTEM_FLAG_DRAW_MAP = 0x00000002,
    /* A level waits for a thread or a fade: world_map_read_pad publishes no pad input. */
    SYSTEM_FLAG_INPUT_BLOCKED = 0x00000004,
    /* Set by world_map_fade_start, cleared by world_map_fade_step one step before the fade ends. */
    SYSTEM_FLAG_FADING = 0x00000008,
    /* world_map_project_dots hides every map dot. */
    SYSTEM_FLAG_HIDE_MAP_DOTS = 0x00000010,
    /* world_map_draw_routes draws no route. */
    SYSTEM_FLAG_HIDE_MAP_ROUTES = 0x00000020,
    /* world_map_entrypoint masks the display for the frame. */
    SYSTEM_FLAG_DISPLAY_OFF = 0x00000040,
    /* world_map_run_world_menus returns without running the menu frame; no code sets it. */
    SYSTEM_FLAG_SKIP_MENU_FRAME = 0x00000080,
    /* Set by world_map_step_brave_story_request: the return level opens the Brave Story screen. */
    SYSTEM_FLAG_REQUEST_BRAVE_STORY = 0x00000100,
    /* world_map_draw_frame does not load the scratch image into the display buffer. */
    SYSTEM_FLAG_NO_BACKGROUND_IMAGE = 0x00000200,
    /* window_dispatch_record_list draws nothing and world_map_pulse_dots returns. */
    SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS = 0x00000400,
    /* hud_draw_value draws nothing. */
    SYSTEM_FLAG_HIDE_CONTEXT_VALUE = 0x00000800,
    /* calendar_draw_date draws nothing. */
    SYSTEM_FLAG_HIDE_CALENDAR_DATE = 0x00001000,
    /* location_draw_label draws no label; set while a level moves or covers the map view. */
    SYSTEM_FLAG_HIDE_LOCATION_LABEL = 0x00002000,
    /* WORLD's menu chose a map (script variable CURRENT_MAP): the return level restarts the core and travels there. */
    SYSTEM_FLAG_REQUEST_DESTINATION = 0x00004000,
    /* travel_step rolled an encounter: world_map_entrypoint returns 1. */
    SYSTEM_FLAG_RANDOM_BATTLE = 0x00008000,
    /* Set after the card menu returns 1: world_map_entrypoint restarts its loop instead of returning. */
    SYSTEM_FLAG_RESTART = 0x00010000,
    /* The unit-panel threads (8, 9 and 12) run: the menu frame draws their windows, not the formation menu. */
    SYSTEM_FLAG_UNIT_PANEL_THREADS = 0x00020000,
    /* A story event replays: world_map_entrypoint returns 2 and the next entry reopens the story-event list. */
    SYSTEM_FLAG_STORY_EVENT_REPLAY = 0x00040000,
    /* Set by title_update_menu: the world entry opens the tutorial menu, which leaves through RETURN_TO_TITLE. */
    SYSTEM_FLAG_TITLE_TUTORIAL = 0x00080000,
    /* Set by world_map_step_tutorial_request: the return level opens the tutorial menu. */
    SYSTEM_FLAG_REQUEST_TUTORIAL = 0x00100000,
    /* A tutorial event plays: world_map_entrypoint returns 2 with the music kept; the next entry reopens its list. */
    SYSTEM_FLAG_TUTORIAL_EVENT = 0x00200000,
    /* world_map_init_state sets the rumor, path, location and story-event variables; no code sets it. */
    SYSTEM_FLAG_UNLOCK_ALL = 0x00400000,
    /* Set by world_map_step_debug_variable_list_request: the return level opens the variable list. */
    SYSTEM_FLAG_REQUEST_VARIABLE_LIST = 0x00800000,
    /* Set by world_map_step_fixed_number_panel_request: the return level opens the fixed number panel. */
    SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL = 0x01000000,
    /* The location menu chose a deep-dungeon battle: world_map_entrypoint returns 4. */
    SYSTEM_FLAG_DEEP_DUNGEON_BATTLE = 0x02000000,
    /* Set by world_map_clear_screen_and_set_clip_rect; the return level then recaptures the world frame. */
    SYSTEM_FLAG_SCREEN_CLEARED = 0x04000000,
    /* Map dots stop pulsing and window_draw_animated_record stops advancing frames. */
    SYSTEM_FLAG_FREEZE_ANIMATION = 0x08000000,
    /* world_map_entrypoint returns 5: system_run_game_loop goes back to OPENING. */
    SYSTEM_FLAG_RETURN_TO_TITLE = 0x10000000,
    /* The world entry opens the Brave Story screen, which leaves through RETURN_TO_TITLE; no code sets it. */
    SYSTEM_FLAG_TITLE_BRAVE_STORY = 0x20000000,
} system_flags_e;

/* g_battle_game_flow_state: what follows when the BATTLE.BIN loop closes a
 * battle or event. */
typedef enum system_game_flow {
    SYSTEM_GAME_FLOW_BATTLE = 0, /* stay in battle_state_run_game_loop for the next event */
    SYSTEM_GAME_FLOW_WORLD = 1,  /* back to the world map */
    SYSTEM_GAME_FLOW_RESET = 2,  /* stop display and audio and restart from the opening */
    SYSTEM_GAME_FLOW_ENDING = 3, /* run the ending, then restart */
} system_game_flow_e;

/* g_system_frontend_world_result: why world_map_entrypoint returned, which tells BATTLE.BIN what
 * to run next. */
typedef enum world_map_result {
    WORLD_MAP_RESULT_NONE = 0, /* no special case: the current scenario's event or battle */
    WORLD_MAP_RESULT_RANDOM_BATTLE = 1,
    WORLD_MAP_RESULT_EVENT = 2, /* a story event replay or a tutorial */
    WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE = 4,
    WORLD_MAP_RESULT_TITLE = 5, /* back to the title, skipping the opening movie */
} world_map_result_e;

/* system_flags_e bits; cleared by save_init_data_tables. */
extern u32 g_system_flags;
extern s32 g_system_frontend_world_result; /* world_map_result_e */
extern u32 g_system_game_loop_stack_pointer;
extern u8 g_system_go_straight_to_battle;

/* Empty (nop, jr ra): every main loop and every blocking wait calls it once per frame. */
void system_frame_stub(void);
void system_reset_game(void);
void system_store_stack_pointer(u32* destination);
void system_restore_game_loop_stack_pointer(u32* source);

/* frame timing */
extern s32 g_system_session_frames;
extern s32 g_system_vsync_interval;
extern s32 g_system_min_vsync_interval;
extern s32 g_system_frame_pacing_suppressed;
extern s32 g_system_frame_pacing_timer;
extern s32 g_system_battle_frame_hsync_count;
extern s32 g_system_battle_frame_hsync_count_max;

void system_handle_draw_sync(void);
void system_handle_vsync(void);

/* play time */
extern s32 g_save_play_time_frames;
extern s32 g_save_play_time_hours;
extern s32 g_save_play_time_minutes;
extern s32 g_save_play_time_seconds;

/* errors */
/* Error codes passed to system_handle_error, whose report hook is empty
 * in retail. */
typedef enum system_error {
    SYSTEM_ERROR_HEAP_ALLOC = 1,            /* file_load_to_heap got no block */
    SYSTEM_ERROR_FILE_LOAD = 2,             /* a checked file load returned 0 */
    SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT = 6,  /* battle_unit_claim_slot found none free */
    SYSTEM_ERROR_SHP_FRAME = 8,             /* frame index above 0x200 or frame pointer -1 */
    SYSTEM_ERROR_SEQ_ANIMATION = 9,         /* animation index above 0x100 or sequence pointer -1 */
    SYSTEM_ERROR_FRAME_PART_COUNT = 10,     /* more than eight parts in a unit frame */
    SYSTEM_ERROR_NO_SPRITESHEET_SLOT = 11,  /* no free or reusable spritesheet slot */
    SYSTEM_ERROR_SEQ_OPCODE = 13,           /* unknown sequence opcode, or a queued animation for slot 0 */
    SYSTEM_ERROR_SMD_LOAD = 17,             /* no SMD buffer or the SMD load could not start */
    SYSTEM_ERROR_DISTORTION_ANIMATION = 23, /* distortion animation id without a handler */
} system_error_e;

/* Error codes BATTLE passes to system_handle_pointer_exception. */
typedef enum system_pointer_exception {
    SYSTEM_POINTER_EXCEPTION_NO_UNIT = 12,            /* a unit lookup by id found no runtime record */
    SYSTEM_POINTER_EXCEPTION_ATTACK_PREPARATION = 19, /* an attack preparation result with no handler */
    SYSTEM_POINTER_EXCEPTION_TURN_EVENT = 20,         /* a turn event type with no handler */
} system_pointer_exception_e;

/* system_handle_battle_load_exception values: a failure base plus the battle id of the unit
 * being set up, or MAP_RETRY alone. */
typedef enum system_battle_load_exception {
    SYSTEM_BATTLE_LOAD_EXCEPTION_MAP_RETRY = 0x80, /* a map tile-data retry gave up after 1000 tries */
    SYSTEM_BATTLE_LOAD_EXCEPTION_NO_SPRITESHEET_SLOT = 100,
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENEMY_LIMIT = 200,
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_PLACEMENT = 300,  /* an ENTD unit's placement was rejected */
    SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_PLACEMENT = 400, /* a deployed unit's placement was rejected */
    SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_UNIT_LOAD = 500, /* a deployed unit's roster load failed */
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_UNIT_INIT = 600,
} system_battle_load_exception_e;

extern int g_system_graphics_error;

void system_handle_error(int error_code);
void system_handle_battle_load_exception(s32 value);
void system_handle_pointer_exception(int error_code);
void system_report_error(s32 allocator_id, s32 error_code);
/* A return-only hook. Main and overlay callers pass two 32-bit arguments, but
 * neither their original meaning nor the hook's original name is proven. */
void system_report_stub(s32 category, s32 code);

/* debug */
extern s32 g_debug_display_enabled;
/* Cleared with the debug display flag; nothing reads it. */
extern s32 g_debug_display_unread_flag; /* cleared with the debug display flag; never read */
extern s32 g_debug_font_print_enabled;

/* input */
/* L1 + R1 + Select + Start restarts the game. */
enum {
    INPUT_RESET_BUTTONS = PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START,
};

extern u8 g_input_repeat_initial_delay;
extern u8 g_input_repeat_period;
extern u8 g_input_secondary_repeat_period;
/* Set to 1 with the menu scroll timings; nothing reads it. */
extern u8 g_menu_scroll_unread_setting; /* set to 1 with the input repeat timings; never read */
extern s32 g_input_pressed_buttons;
extern u32 g_input_released_buttons;
extern u32 g_input_pressed_copy_0;
extern u32 g_input_pressed_copy_1;
extern u32 g_input_pressed_copy_2;
extern u32 g_input_pressed_copy_3;
extern u32 g_input_previous_copy_0;
extern u32 g_input_previous_copy_1;
extern u32 g_input_previous_copy_2;
extern u32 g_input_previous_copy_3;
extern u32 g_input_stable_frames;

/* options and menu scrolling */
extern game_options_t g_game_options;
extern game_options_t g_game_options_defaults;
extern u8 g_menu_scroll_accel_delay;
extern u8 g_menu_scroll_slow_step;
extern u8 g_menu_scroll_fast_step;

/* scenario */
/* The script variables: 32-bit values first (event_script_variable_id_e), then the packed flags. */
extern s32 g_script_variable_storage[0x100];
extern u16 g_scenario_condition_offsets[];
extern s16 g_scenario_condition_data[];
extern u16 g_scenario_event_finish_operations[];
extern event_file_block_t g_script_event_block_buffer[];

/* utilities */
s32 random_roll_pass_fail(s32 maximum, s32 threshold);
void vector_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void vector_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);
void action_copy_data(const action_command_t* source, action_command_t* destination);

/* Source-first byte copies; nonpositive counts leave the destination unchanged. */
void mem_copy_bytes_source_first(const void* source, void* destination, int count);
void mem_copy_byte_data_source_first(const void* source, void* destination, s32 count);
/* Clear count bytes; nonpositive counts leave the destination unchanged. */
void mem_zero(void* destination, s32 count);

/* 4 KB of zero words after the C start-up, run as no-ops; returns at once. */
void system_zero_padding(void);

#endif

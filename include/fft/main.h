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
/* Bits of g_main_system_flags, the 32-bit control flags of the WLDCORE world-map
 * loop: frame state, what the frame leaves out, requests left by WORLD and
 * OPENING, and the reason the loop ends. */
typedef enum main_system_flags {
    /* wldcore_entrypoint runs frames while set; wldcore_init_world_map_state sets it, a level clears it to leave. */
    MAIN_SYSTEM_FLAG_RUNNING = 0x00000001,
    /* Draw this frame with the 3D map; wldcore_gfx_draw_world_frame_with_map clears it after drawing. */
    MAIN_SYSTEM_FLAG_DRAW_MAP = 0x00000002,
    /* A level waits for a thread or a fade: wldcore_input_update_controller publishes no pad input. */
    MAIN_SYSTEM_FLAG_INPUT_BLOCKED = 0x00000004,
    /* Set by wldcore_fade_start_screen, cleared by wldcore_fade_step_screen one step before the fade ends. */
    MAIN_SYSTEM_FLAG_FADING = 0x00000008,
    /* wldcore_map_project_and_cull_dots hides every map dot. */
    MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS = 0x00000010,
    /* wldcore_map_draw_visible_routes draws no route. */
    MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES = 0x00000020,
    /* wldcore_entrypoint masks the display for the frame. */
    MAIN_SYSTEM_FLAG_DISPLAY_OFF = 0x00000040,
    /* wldcore_menu_run_world_frame returns without running the menu frame; no code sets it. */
    MAIN_SYSTEM_FLAG_SKIP_MENU_FRAME = 0x00000080,
    /* Set by world_formation_update_world_map_root_request: the return level opens the Brave Story screen. */
    MAIN_SYSTEM_FLAG_REQUEST_BRAVE_STORY = 0x00000100,
    /* wldcore_gfx_draw_world_frame does not load the scratch image into the display buffer. */
    MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE = 0x00000200,
    /* window_dispatch_record_list draws nothing and wldcore_map_pulse_dot_colors returns. */
    MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS = 0x00000400,
    /* wldcore_gfx_draw_context_value_display draws nothing. */
    MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE = 0x00000800,
    /* wldcore_gfx_draw_calendar_date draws nothing. */
    MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE = 0x00001000,
    /* wldcore_proposition_submit_marker_request draws no marker; set while a level moves or covers the map view. */
    MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER = 0x00002000,
    /* WORLD's menu chose a map (script variable CURRENT_MAP): the return level restarts the core and travels there. */
    MAIN_SYSTEM_FLAG_REQUEST_DESTINATION = 0x00004000,
    /* wldcore_map_step_route_travel rolled an encounter: wldcore_entrypoint returns 1. */
    MAIN_SYSTEM_FLAG_RANDOM_BATTLE = 0x00008000,
    /* Set after the card menu returns 1: wldcore_entrypoint restarts its loop instead of returning. */
    MAIN_SYSTEM_FLAG_RESTART = 0x00010000,
    /* The unit-panel threads (8, 9 and 12) run: the menu frame draws their windows, not the formation menu. */
    MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS = 0x00020000,
    /* A story event replays: wldcore_entrypoint returns 2 and the next entry reopens the story-event list. */
    MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY = 0x00040000,
    /* Set by open_title_update_menu: the world entry opens the tutorial menu, which leaves through RETURN_TO_TITLE. */
    MAIN_SYSTEM_FLAG_TITLE_TUTORIAL = 0x00080000,
    /* Set by world_formation_update_tutorial_categories_request: the return level opens the tutorial menu. */
    MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL = 0x00100000,
    /* A tutorial event plays: wldcore_entrypoint returns 2 with the music kept; the next entry reopens its list. */
    MAIN_SYSTEM_FLAG_TUTORIAL_EVENT = 0x00200000,
    /* wldcore_init_world_map_state sets the rumor, path, location and story-event variables; no code sets it. */
    MAIN_SYSTEM_FLAG_UNLOCK_ALL = 0x00400000,
    /* Set by world_formation_update_debug_variable_list_request: the return level opens the variable list. */
    MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST = 0x00800000,
    /* Set by world_formation_update_fixed_number_panel_request: the return level opens the fixed number panel. */
    MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL = 0x01000000,
    /* The location menu chose a deep-dungeon battle: wldcore_entrypoint returns 4. */
    MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE = 0x02000000,
    /* Set by wldcore_clear_screen_and_set_map_clip_rect; the return level then recaptures the world frame. */
    MAIN_SYSTEM_FLAG_SCREEN_CLEARED = 0x04000000,
    /* Map dots stop pulsing and wldcore_gfx_draw_animated_display_object stops advancing frames. */
    MAIN_SYSTEM_FLAG_FREEZE_ANIMATION = 0x08000000,
    /* wldcore_entrypoint returns 5: main_system_run_game_loop goes back to OPENING. */
    MAIN_SYSTEM_FLAG_RETURN_TO_TITLE = 0x10000000,
    /* The world entry opens the Brave Story screen, which leaves through RETURN_TO_TITLE; no code sets it. */
    MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY = 0x20000000,
} main_system_flags_e;

/* g_battle_game_flow_state: what follows when the BATTLE.BIN loop closes a
 * battle or event. */
typedef enum main_game_flow_state {
    MAIN_GAME_FLOW_BATTLE = 0, /* stay in battle_state_run_game_loop for the next event */
    MAIN_GAME_FLOW_WORLD = 1,  /* back to the world map */
    MAIN_GAME_FLOW_RESET = 2,  /* stop display and audio and restart from the opening */
    MAIN_GAME_FLOW_ENDING = 3, /* run the ending, then restart */
} main_game_flow_state_e;

/* main_system_flags_e bits; cleared by main_save_init_data_tables. */
extern u32 g_main_system_flags;
extern s32 g_main_system_frontend_world_result;
extern u32 g_main_system_game_loop_stack_pointer;
extern u8 g_main_system_go_straight_to_battle;

/* Empty (nop, jr ra): every main loop and every blocking wait calls it once per frame. */
void main_system_frame_stub(void);
void main_system_reset_game(void);
void main_system_store_stack_pointer(u32* destination);
void main_restore_game_loop_stack_pointer(u32* source);

/* frame timing */
extern s32 g_main_system_session_frames;
extern s32 g_main_system_vsync_interval;
extern s32 g_main_system_min_vsync_interval;
extern s32 g_main_frame_pacing_suppressed;
extern s32 g_main_frame_pacing_timer;
extern s32 g_main_system_battle_frame_hsync_count;
extern s32 g_main_system_battle_frame_hsync_count_max;

void main_system_handle_draw_sync_callback(void);
void main_system_handle_vsync_callback(void);

/* play time */
extern s32 g_main_system_play_time_frames;
extern s32 g_main_system_play_time_hours;
extern s32 g_main_system_play_time_minutes;
extern s32 g_main_system_play_time_seconds;

/* errors */
/* Error codes passed to main_system_handle_error, whose report hook is empty
 * in retail. */
typedef enum main_system_error {
    MAIN_SYSTEM_ERROR_HEAP_ALLOC = 1,            /* main_file_load_to_heap got no block */
    MAIN_SYSTEM_ERROR_FILE_LOAD = 2,             /* a checked file load returned 0 */
    MAIN_SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT = 6,  /* battle_unit_claim_slot found none free */
    MAIN_SYSTEM_ERROR_SHP_FRAME = 8,             /* frame index above 0x200 or frame pointer -1 */
    MAIN_SYSTEM_ERROR_SEQ_ANIMATION = 9,         /* animation index above 0x100 or sequence pointer -1 */
    MAIN_SYSTEM_ERROR_FRAME_PART_COUNT = 10,     /* more than eight parts in a unit frame */
    MAIN_SYSTEM_ERROR_NO_SPRITESHEET_SLOT = 11,  /* no free or reusable spritesheet slot */
    MAIN_SYSTEM_ERROR_SEQ_OPCODE = 13,           /* unknown sequence opcode, or a queued animation for slot 0 */
    MAIN_SYSTEM_ERROR_SMD_LOAD = 17,             /* no SMD buffer or the SMD load could not start */
    MAIN_SYSTEM_ERROR_DISTORTION_ANIMATION = 23, /* distortion animation id without a handler */
} main_system_error_e;

extern int g_main_system_graphics_error;

void main_system_handle_error(int error_code);
void main_system_handle_battle_load_exception(s32 value);
void main_system_handle_pointer_exception(int error_code);
void main_system_report_error(s32 allocator_id, s32 error_code);
/* A return-only hook. Main and overlay callers pass two 32-bit arguments, but
 * neither their original meaning nor the hook's original name is proven. */
void main_system_report_stub(s32 category, s32 code);

/* debug */
extern s32 g_main_debug_display_enabled;
/* Cleared with the debug display flag; nothing reads it. */
extern s32 g_main_debug_display_unread_flag; /* cleared with the debug display flag; never read */
extern s32 g_main_font_print_enabled;

/* input */
/* L1 + R1 + Select + Start restarts the game. */
enum {
    MAIN_INPUT_RESET_BUTTONS = PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START,
};

extern u8 g_main_input_repeat_initial_delay;
extern u8 g_main_input_repeat_period;
extern u8 g_main_input_secondary_repeat_period;
/* Set to 1 with the menu scroll timings; nothing reads it. */
extern u8 g_main_menu_scroll_unread_setting; /* set to 1 with the input repeat timings; never read */
extern s32 g_main_input_pressed_buttons;
extern u32 g_main_input_released_buttons;
extern u32 g_main_input_pressed_copy_0;
extern u32 g_main_input_pressed_copy_1;
extern u32 g_main_input_pressed_copy_2;
extern u32 g_main_input_pressed_copy_3;
extern u32 g_main_input_previous_copy_0;
extern u32 g_main_input_previous_copy_1;
extern u32 g_main_input_previous_copy_2;
extern u32 g_main_input_previous_copy_3;
extern u32 g_main_input_stable_frames;

/* options and menu scrolling */
extern game_options_t g_main_game_options;
extern game_options_t g_main_game_options_defaults;
extern u8 g_main_menu_scroll_accel_delay;
extern u8 g_main_menu_scroll_slow_step;
extern u8 g_main_menu_scroll_fast_step;

/* scenario */
/* The script variables: 32-bit values first (event_script_variable_id_e), then the packed flags. */
extern s32 g_main_script_variables[0x100];
extern u16 g_main_scenario_condition_offsets[];
extern s16 g_main_scenario_condition_data[];
extern u16 g_main_scenario_event_finish_operations[];
extern event_file_block_t g_main_script_event_block[];

/* job lookups */
/* The menu library's job helpers (src/event/menu), compiled into WORLD and the
 * EVENT overlays. */
void job_calculate_and_store_unlocks(party_unit_t* party, u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]);
s32 job_find_first_for_skillset(s32 skillset);
s32 job_get_generic_index(s32 job_id);
s32 job_get_skillset(s32 job_id);
s32 job_is_special_monster(s32 job_id);

/* utilities */
s32 main_util_roll_pass_fail(s32 maximum, s32 threshold);
void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void main_util_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);
void main_util_copy_action_data(const action_command_t* source, action_command_t* destination);

/* Source-first byte copies; nonpositive counts leave the destination unchanged. */
void main_mem_copy_bytes_source_first(const void* source, void* destination, int count);
void main_mem_copy_byte_data_source_first(const void* source, void* destination, s32 count);
/* Clear count bytes; nonpositive counts leave the destination unchanged. */
void main_mem_zero(void* destination, s32 count);

/* 4 KB of zero words after the C start-up, run as no-ops; returns at once. */
void main_run_zero_padding(void);

#endif

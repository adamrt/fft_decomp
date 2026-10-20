#ifndef SYSTEM_H
#define SYSTEM_H

/* System: MAIN's game loop and its control flags, frame timing, error reporting, debug switches, the boot logos and the
 * Now Loading message, BATTLE's frame pacing, and OPEN's main-loop flags. */

#include "psx/types.h"

/* game loop */
/* Bits of g_system_flags, the 32-bit control flags of the WLDCORE world-map
 * loop: frame state, what the frame leaves out, requests left by WORLD and
 * OPENING, and the reason the loop ends. */
typedef enum system_flags {
    /* wldcore_entrypoint runs frames while set; map_state_init sets it, a level clears it to leave. */
    SYSTEM_FLAG_RUNNING = 0x00000001,
    /* Draw this frame with the 3D map; map_frame_draw clears it after drawing. */
    SYSTEM_FLAG_DRAW_MAP = 0x00000002,
    /* A level waits for a thread or a fade: world_map_pad_read publishes no pad input. */
    SYSTEM_FLAG_INPUT_BLOCKED = 0x00000004,
    /* Set by world_map_fade_start, cleared by world_map_fade_update one step before the fade ends. */
    SYSTEM_FLAG_FADING = 0x00000008,
    /* world_map_project_dots hides every map dot. */
    SYSTEM_FLAG_HIDE_MAP_DOTS = 0x00000010,
    /* world_map_routes_draw draws no route. */
    SYSTEM_FLAG_HIDE_MAP_ROUTES = 0x00000020,
    /* wldcore_entrypoint masks the display for the frame. */
    SYSTEM_FLAG_DISPLAY_OFF = 0x00000040,
    /* world_map_menus_update returns without running the menu frame; no code sets it. */
    SYSTEM_FLAG_SKIP_MENU_FRAME = 0x00000080,
    /* Set by brave_story_request_update: the return level opens the Brave Story screen. */
    SYSTEM_FLAG_REQUEST_BRAVE_STORY = 0x00000100,
    /* world_map_frame_draw does not load the scratch image into the display buffer. */
    SYSTEM_FLAG_NO_BACKGROUND_IMAGE = 0x00000200,
    /* window_dispatch_gs_record_list draws nothing and world_map_pulse_dots returns. */
    SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS = 0x00000400,
    /* hud_value_draw draws nothing. */
    SYSTEM_FLAG_HIDE_CONTEXT_VALUE = 0x00000800,
    /* calendar_date_draw draws nothing. */
    SYSTEM_FLAG_HIDE_CALENDAR_DATE = 0x00001000,
    /* location_label_draw draws no label; set while a level moves or covers the map view. */
    SYSTEM_FLAG_HIDE_LOCATION_LABEL = 0x00002000,
    /* WORLD's menu chose a map (script variable CURRENT_MAP): the return level restarts the core and travels there. */
    SYSTEM_FLAG_REQUEST_DESTINATION = 0x00004000,
    /* travel_step rolled an encounter: wldcore_entrypoint returns 1. */
    SYSTEM_FLAG_RANDOM_BATTLE = 0x00008000,
    /* Set after the card menu returns 1: wldcore_entrypoint restarts its loop instead of returning. */
    SYSTEM_FLAG_RESTART = 0x00010000,
    /* The unit-panel threads (8, 9 and 12) run: the menu frame draws their windows, not the formation menu. */
    SYSTEM_FLAG_UNIT_PANEL_THREADS = 0x00020000,
    /* A story event replays: wldcore_entrypoint returns 2 and the next entry reopens the story-event list. */
    SYSTEM_FLAG_STORY_EVENT_REPLAY = 0x00040000,
    /* Set by main_menu_update: the world entry opens the tutorial menu, which leaves through RETURN_TO_TITLE. */
    SYSTEM_FLAG_TITLE_TUTORIAL = 0x00080000,
    /* Set by tutorial_request_update: the return level opens the tutorial menu. */
    SYSTEM_FLAG_REQUEST_TUTORIAL = 0x00100000,
    /* A tutorial event plays: wldcore_entrypoint returns 2 with the music kept; the next entry reopens its list. */
    SYSTEM_FLAG_TUTORIAL_EVENT = 0x00200000,
    /* map_state_init sets the rumor, path, location and story-event variables; no code sets it. */
    SYSTEM_FLAG_UNLOCK_ALL = 0x00400000,
    /* Set by variable_list_request_update: the return level opens the variable list. */
    SYSTEM_FLAG_REQUEST_VARIABLE_LIST = 0x00800000,
    /* Set by party_number_request_update: opens the debug Party number panel. */
    SYSTEM_FLAG_REQUEST_DEBUG_PARTY_NUMBER = 0x01000000,
    /* The location menu chose a deep-dungeon battle: wldcore_entrypoint returns 4. */
    SYSTEM_FLAG_DEEP_DUNGEON_BATTLE = 0x02000000,
    /* Set by world_map_clear_screen_and_set_clip_rect; the return level then recaptures the world frame. */
    SYSTEM_FLAG_SCREEN_CLEARED = 0x04000000,
    /* Map dots stop pulsing and window_gs_animated_record_draw stops advancing frames. */
    SYSTEM_FLAG_FREEZE_ANIMATION = 0x08000000,
    /* wldcore_entrypoint returns 5: system_game_loop_run goes back to OPENING. */
    SYSTEM_FLAG_RETURN_TO_TITLE = 0x10000000,
    /* The world entry opens the Brave Story screen, which leaves through RETURN_TO_TITLE; no code sets it. */
    SYSTEM_FLAG_TITLE_BRAVE_STORY = 0x20000000
} system_flags_e;

/* g_game_flow_state: what follows when the BATTLE.BIN loop closes a
 * battle or event. */
typedef enum system_game_flow {
    SYSTEM_GAME_FLOW_BATTLE = 0, /* stay in battle_entrypoint for the next event */
    SYSTEM_GAME_FLOW_WORLD = 1,  /* back to the world map */
    SYSTEM_GAME_FLOW_RESET = 2,  /* stop display and audio and restart from the opening */
    SYSTEM_GAME_FLOW_ENDING = 3  /* run the ending, then restart */
} system_game_flow_e;

/* BATTLE reloads this result with its program image; MAIN reads it after battle_entrypoint returns. */
extern system_game_flow_e g_game_flow_state;
extern u32 g_system_flags; /* system_flags_e bits; cleared by save_data_tables_init */
extern u8 g_system_go_straight_to_battle;

/* Also entered by the native stack-restoring reset routine. */
void system_game_loop_run(void);
void system_game_reset(void);
void system_stack_pointer_store(u32* destination);
void system_game_loop_stack_restore(u32* source);

/* Empty (nop, jr ra): every main loop and every blocking wait calls it once per frame. */
void system_frame_stub(void);

/* frame timing */
/* Resettable VBlank counter used by CD waits and the title exit. */
extern s32 g_system_wait_frames;
extern s32 g_system_vsync_interval;
extern s32 g_system_min_vsync_interval;
extern s32 g_system_saved_vsync_interval;
extern s32 g_system_frame_pacing_suppressed;
extern s32 g_system_frame_pacing_timer;
extern s32 g_system_battle_frame_hsync_count;

void system_draw_sync_callback(void);
void system_vsync_callback(void);

/* BATTLE frame pacing */
s32 system_vsync_interval_get(void);
s32 system_vsync_interval_set(s32 speed);
void system_min_vsync_interval_set(s32 value);
void system_min_vsync_interval_clear(void);
void system_frame_pacing_reset(void);

/* errors */
/* Error codes passed to system_error_handle, whose report hook is empty
 * in retail. */
typedef enum system_error {
    SYSTEM_ERROR_HEAP_ALLOC = 1,           /* heap_file_load got no block */
    SYSTEM_ERROR_FILE_LOAD = 2,            /* a checked file load returned 0 */
    SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT = 6, /* runtime_slot_claim found none free */
    SYSTEM_ERROR_SHP_FRAME = 8,            /* frame index above 0x200 or frame pointer -1 */
    SYSTEM_ERROR_SEQ_ANIMATION = 9,        /* animation index above 0x100 or sequence pointer -1 */
    SYSTEM_ERROR_FRAME_PART_COUNT = 10,    /* more than eight parts in a unit frame */
    SYSTEM_ERROR_NO_SPRITESHEET_SLOT = 11, /* no free or reusable spritesheet slot */
    SYSTEM_ERROR_SEQ_OPCODE = 13,          /* unknown sequence opcode, or a queued animation for slot 0 */
    SYSTEM_ERROR_SMD_LOAD = 17,            /* no SMD buffer or the SMD load could not start */
    SYSTEM_ERROR_DISTORTION_ANIMATION = 23 /* distortion animation id without a handler */
} system_error_e;

/* Error codes BATTLE passes to system_pointer_exception_handle. */
typedef enum system_pointer_exception {
    SYSTEM_POINTER_EXCEPTION_NO_UNIT = 12,            /* a unit lookup by id found no runtime record */
    SYSTEM_POINTER_EXCEPTION_ATTACK_PREPARATION = 19, /* an attack preparation result with no handler */
    SYSTEM_POINTER_EXCEPTION_TURN_EVENT = 20          /* a turn event type with no handler */
} system_pointer_exception_e;

/* system_battle_load_exception_handle values: a failure base plus the battle id of the unit
 * being set up, or BATTLE_MAP_RETRY alone. */
typedef enum system_battle_load_exception {
    SYSTEM_BATTLE_LOAD_EXCEPTION_MAP_RETRY = 0x80, /* a map tile-data retry gave up after 1000 tries */
    SYSTEM_BATTLE_LOAD_EXCEPTION_NO_SPRITESHEET_SLOT = 100,
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENEMY_LIMIT = 200,
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_PLACEMENT = 300,  /* an ENTD unit's placement was rejected */
    SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_PLACEMENT = 400, /* a deployed unit's placement was rejected */
    SYSTEM_BATTLE_LOAD_EXCEPTION_PARTY_UNIT_LOAD = 500, /* a deployed unit's roster load failed */
    SYSTEM_BATTLE_LOAD_EXCEPTION_ENTD_UNIT_INIT = 600
} system_battle_load_exception_e;

void system_error_handle(int error_code);
void system_battle_load_exception_handle(s32 value);
void system_pointer_exception_handle(int error_code);
void system_error_report(s32 allocator_id, s32 error_code);

/* A return-only hook. Main and overlay callers pass two 32-bit arguments, but
 * neither their original meaning nor the hook's original name is proven. */
void system_report_stub(s32 category, s32 code);

/* debug */
/* Provisional: the debug VARIABLE_LIST level pushed by variable_list_open;
 * variable_list_draw draws row_count variables from first_variable into the render record. */
typedef struct debug_variable_list_level {
    s32 window_index;   /* 0x00; cursor window */
    s32 render_index;   /* 0x04 */
    s32 first_variable; /* 0x08 */
    s32 cursor;         /* 0x0c; cursor row */
    u8 _unused_10[0x20];
    s32 row_count; /* 0x30 */
} debug_variable_list_level_t;

extern s32 g_debug_display_enabled;
extern s32 g_debug_display_unread_flag; /* cleared with the debug display flag; never read */
extern s32 g_debug_font_print_enabled;

void debug_font_load(void);

/* boot */
void boot_startup_run(void);
void boot_session_reset(void);
void boot_sceap_logo_show(void);
void boot_squaresoft_logo_fade_in(void);
void boot_squaresoft_logo_fade_out(void);

/* 4 KB of zero words after the C start-up, run as no-ops; returns at once. */
void system_zero_padding(void);

/* Now Loading */
extern s32 g_loading_display_frame_counter;

void loading_message_build(u32 visible, s32 x, s32 y);
void loading_centered_message_build(u32 visible);
void loading_otag_append(u32* otag);
void loading_message_draw(void);

/* runtime flags */
/* Bits of g_system_runtime_flags. */
typedef enum system_runtime_flag {
    SYSTEM_RUNTIME_FLAG_RUNNING = 0x00001,          /* the OPEN main loop runs while set */
    SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING = 0x00002,  /* cleared by movie_stream_stop */
    SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING = 0x00004, /* movie_cd_audio_pause pauses the drive and clears it */
    SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN = 0x00008,
    SYSTEM_RUNTIME_FLAG_INPUT_DISABLED = 0x00010,
    SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED = 0x00040,
    SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED = 0x00080,
    SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN = 0x00100,
    SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME = 0x00400, /* the WORLD menu frame runs every loop */
    SYSTEM_RUNTIME_FLAG_WIDE_SCREEN = 0x01000,
    SYSTEM_RUNTIME_FLAG_RGB24 = 0x02000,
    NEW_GAME_PARTY_MODE_FLAG_BASE = 0x04000, /* shifted left by the party mode: which new-game party was built */
    NEW_GAME_PARTY_MODE_FLAG_MASK = 0x1c000,
    /* movie_present_frame: clears the frame, or copies it from VRAM (512, 256), instead of
     * uploading the file image. */
    SYSTEM_RUNTIME_FLAG_CLEAR_FRAME = 0x20000,
    SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM = 0x40000,
    SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED = 0x80000
} system_runtime_flag_e;

extern u32 g_system_runtime_flags;
extern s32 g_system_vsync_mode;
extern u8 g_system_work_buffer_0[];
extern u8 g_system_work_buffer_1[];

#endif

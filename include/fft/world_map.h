#ifndef WORLD_MAP_H
#define WORLD_MAP_H

/* The World Map the player travels on (the game's own term, not the WORLD program): its entry and display frame, fades,
 * value display, pictures, tiles, camera and scrolling, map dots and routes, the menu stack levels and transitions it
 * pushes, and WORLD's world map menu. */

#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/menu_list.h"
#include "fft/menu_stack.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* map loop flags */
/* Bits of g_world_map_flags, the 32-bit control flags of the WLDCORE world-map
 * loop: frame state, what the frame leaves out, requests left by WORLD and
 * OPENING, and the reason the loop ends. */
typedef enum world_map_flags {
    /* wldcore_entrypoint runs frames while set; map_state_init sets it, a level clears it to leave. */
    WORLD_MAP_FLAG_RUNNING = 0x00000001,
    /* Draw this frame with the 3D map; map_frame_draw clears it after drawing. */
    WORLD_MAP_FLAG_DRAW_MAP = 0x00000002,
    /* A level waits for a thread or a fade: world_map_pad_read publishes no pad input. */
    WORLD_MAP_FLAG_INPUT_BLOCKED = 0x00000004,
    /* Set by world_map_fade_start, cleared by world_map_fade_update one step before the fade ends. */
    WORLD_MAP_FLAG_FADING = 0x00000008,
    /* world_map_project_dots hides every map dot. */
    WORLD_MAP_FLAG_HIDE_MAP_DOTS = 0x00000010,
    /* world_map_routes_draw draws no route. */
    WORLD_MAP_FLAG_HIDE_MAP_ROUTES = 0x00000020,
    /* wldcore_entrypoint masks the display for the frame. */
    WORLD_MAP_FLAG_DISPLAY_OFF = 0x00000040,
    /* world_map_menus_update returns without running the menu frame; no code sets it. */
    WORLD_MAP_FLAG_SKIP_MENU_FRAME = 0x00000080,
    /* Set by brave_story_request_update: the return level opens the Brave Story screen. */
    WORLD_MAP_FLAG_REQUEST_BRAVE_STORY = 0x00000100,
    /* world_map_frame_draw does not load the scratch image into the display buffer. */
    WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE = 0x00000200,
    /* display_object_gs_list_draw draws nothing and world_map_pulse_dots returns. */
    WORLD_MAP_FLAG_HIDE_DISPLAY_OBJECTS = 0x00000400,
    /* hud_value_draw draws nothing. */
    WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE = 0x00000800,
    /* calendar_date_draw draws nothing. */
    WORLD_MAP_FLAG_HIDE_CALENDAR_DATE = 0x00001000,
    /* location_label_draw draws no label; set while a level moves or covers the map view. */
    WORLD_MAP_FLAG_HIDE_LOCATION_LABEL = 0x00002000,
    /* WORLD's menu chose a map (script variable CURRENT_MAP): the return level restarts the core and travels there. */
    WORLD_MAP_FLAG_REQUEST_DESTINATION = 0x00004000,
    /* travel_step rolled an encounter: wldcore_entrypoint returns 1. */
    WORLD_MAP_FLAG_RANDOM_BATTLE = 0x00008000,
    /* Set after the card menu returns 1: wldcore_entrypoint restarts its loop instead of returning. */
    WORLD_MAP_FLAG_RESTART = 0x00010000,
    /* The unit-panel threads (8, 9 and 12) run: the menu frame draws their windows, not the formation menu. */
    WORLD_MAP_FLAG_UNIT_PANEL_THREADS = 0x00020000,
    /* A story event replays: wldcore_entrypoint returns 2 and the next entry reopens the story-event list. */
    WORLD_MAP_FLAG_STORY_EVENT_REPLAY = 0x00040000,
    /* Set by main_menu_update: the world entry opens the tutorial menu, which leaves through RETURN_TO_TITLE. */
    WORLD_MAP_FLAG_TITLE_TUTORIAL = 0x00080000,
    /* Set by tutorial_request_update: the return level opens the tutorial menu. */
    WORLD_MAP_FLAG_REQUEST_TUTORIAL = 0x00100000,
    /* A tutorial event plays: wldcore_entrypoint returns 2 with the music kept; the next entry reopens its list. */
    WORLD_MAP_FLAG_TUTORIAL_EVENT = 0x00200000,
    /* map_state_init sets the rumor, path, location and story-event variables; no code sets it. */
    WORLD_MAP_FLAG_UNLOCK_ALL = 0x00400000,
    /* Set by variable_list_request_update: the return level opens the variable list. */
    WORLD_MAP_FLAG_REQUEST_VARIABLE_LIST = 0x00800000,
    /* Set by party_number_request_update: opens the debug Party number panel. */
    WORLD_MAP_FLAG_REQUEST_DEBUG_PARTY_NUMBER = 0x01000000,
    /* The location menu chose a deep-dungeon battle: wldcore_entrypoint returns 4. */
    WORLD_MAP_FLAG_DEEP_DUNGEON_BATTLE = 0x02000000,
    /* Set by world_map_clear_screen_and_set_clip_rect; the return level then recaptures the world frame. */
    WORLD_MAP_FLAG_SCREEN_CLEARED = 0x04000000,
    /* Map dots stop pulsing and display_object_gs_animated_draw stops advancing frames. */
    WORLD_MAP_FLAG_FREEZE_ANIMATION = 0x08000000,
    /* wldcore_entrypoint returns 5: system_game_loop_run goes back to OPENING. */
    WORLD_MAP_FLAG_RETURN_TO_TITLE = 0x10000000,
    /* The world entry opens the Brave Story screen, which leaves through RETURN_TO_TITLE; no code sets it. */
    WORLD_MAP_FLAG_TITLE_BRAVE_STORY = 0x20000000
} world_map_flags_e;

extern u32 g_world_map_flags; /* world_map_flags_e bits; cleared by save_data_tables_init */

/* entry */
/* g_system_frontend_world_result: why wldcore_entrypoint returned, which tells BATTLE.BIN what
 * to run next. */
typedef enum world_map_result {
    WORLD_MAP_RESULT_NONE = 0, /* no special case: the current scenario's event or battle */
    WORLD_MAP_RESULT_RANDOM_BATTLE = 1,
    WORLD_MAP_RESULT_EVENT = 2, /* a story event replay or a tutorial */
    WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE = 4,
    WORLD_MAP_RESULT_TITLE = 5 /* back to the title, skipping the opening movie */
} world_map_result_e;

/* Slots that the WLDCORE screens use for threads of their own. */
typedef enum world_map_thread_slot {
    WORLD_MAP_THREAD_SLOT_TRANSITION = 4,    /* the zoom thread of a screen transition */
    WORLD_MAP_THREAD_SLOT_WINDOW_PANEL = 12, /* a list or number panel */
    WORLD_MAP_THREAD_SLOT_MESSAGE = 14       /* the message box and script text */
} world_map_thread_slot_e;

/* MAIN saves WLDCORE's return value here for BATTLE's setup. */
extern world_map_result_e g_system_frontend_world_result;

/* g_world_map_flags as backdrop_update last saw it (world_map_flags_e). */
extern u32 g_world_map_previous_flags;

/* Run WLDCORE's world-map loop and return a world_map_result_e. */
world_map_result_e wldcore_entrypoint(void);

/* MAIN loads WLDCORE and, when requested, WORLD, then enters the map. */
world_map_result_e overlay_world_run(s32 load_world);
void world_map_switch_to_stack(void*);
void world_map_restore_previous_stack(void);
void world_map_menu_state_init(void);

/* Empty; subsystems_init calls it between the map dot and location marker setup. */
void world_map_stub_init(void);

/* input */
extern u32 g_world_map_input_new_button_presses;

void world_map_input_repeat_init(void);
u32 world_map_input_repeat_update(u32 buttons);
void world_map_pad_read(void);

/* display frame */
enum { WORLD_MAP_BACKDROP_WORDS = 256 * 240 / 2 };

extern PACKET g_world_map_primitive_buffers[];

/* The libgs ordering tables of the two display buffers and their tags: the main table of 16 tags and an auxiliary
 * one of 4. world_map_backdrop_mode_set indexes the GsOT arrays with their 0x14-byte stride. */
extern GsOT g_world_map_ordering_tables[2];
extern GsOT g_world_map_aux_ordering_tables[2];

/* Begins with the 256x240 16-bit backdrop image, WORLD_MAP_BACKDROP_WORDS 32-bit words. */
extern u32* g_world_map_scratch_buffer;

void world_map_frame_draw(void);
void world_map_display_rect_set(s32 buffer_index, RECT* rect);
void world_map_image_buffers_clear(s32 clear_scratch);
void world_map_clear_screen_and_set_clip_rect(s32 mode);
void world_map_backdrop_capture(void);
void world_map_backdrop_mode_set(s32 capture);

/* fades */
/* g_world_map_fade.flags[]: 32-bit flags[0] controls the screen fade and its fill
 * box; flags[1] controls a second fill box. world_map_fade_start takes OUT,
 * REDRAW_MAP, STOP_MUSIC and START_MUSIC and adds RUNNING. */
typedef enum world_map_fade_flag {
    WORLD_MAP_FADE_RUNNING = 0x01,
    WORLD_MAP_FADE_OUT = 0x02,         /* fades to black; stays set while the screen is dark */
    WORLD_MAP_FADE_REDRAW_MAP = 0x04,  /* every fade step sets WORLD_MAP_FLAG_DRAW_MAP */
    WORLD_MAP_FADE_BOX_VISIBLE = 0x08, /* the fill box is drawn without a running fade */
    WORLD_MAP_FADE_STOP_MUSIC = 0x10,  /* also fades the music out and unloads it */
    WORLD_MAP_FADE_START_MUSIC = 0x20  /* also loads MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP), fading it in */
} world_map_fade_flag_e;

/* The screen fade and its two full-screen fill boxes. The parallel arrays share one object, which is why the
 * target builds all three walk pointers from a single symbol address. */
typedef struct world_map_fade {
    u32 flags[2];      /* 0x00; world_map_fade_flag_e */
    s32 priorities[2]; /* 0x08; GsSortBoxFill OT priority of each box, read as a 16-bit value */
    s32 elapsed;       /* 0x10; fade frames run, stepped by world_map_fade_update */
    s32 duration;      /* 0x14 */
    GsBOXF boxes[2];   /* 0x18; full-screen fill boxes drawn by world_map_fade_boxes_draw */
} world_map_fade_t;
typedef char world_map_fade_size_must_be_56[sizeof(world_map_fade_t) == 56 ? 1 : -1];

extern world_map_fade_t g_world_map_fade;

/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern rgb24_t g_world_map_brightness_rgb;

void world_map_fade_start(s32 flags, s32 duration);
s32 world_map_fade_update(void);
void world_map_fade_boxes_draw(void);
void world_map_fade_tint_color(CVECTOR* source, CVECTOR* out);

/* value display (HUD) */
/* g_hud_value_mode: the layout of the HUD's value display
 * (hud_value_draw). */
typedef enum hud_value_mode {
    HUD_VALUE_PLAIN = 0,         /* eight digits */
    HUD_VALUE_FUNDS = 1,         /* labeled and underlined; the war funds */
    HUD_VALUE_PLAY_TIME = 2,     /* hours, minutes and seconds */
    HUD_VALUE_LABELED = 3,       /* PLAIN under a small label */
    HUD_VALUE_COST_AND_FUNDS = 4 /* a labeled five-digit secondary value, then FUNDS */
} hud_value_mode_e;

extern s32 g_hud_value_mode;
extern CVECTOR g_hud_color; /* color of the context-value display, tinted by world_map_fade_tint_color */

/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both 32-bit values as one point32_t. */
extern point32_t g_hud_value_position;

/* The values the display shows: the main value, a secondary one, and the seconds of a play time. */
extern s32 g_hud_value;
extern s32 g_hud_value_secondary;
extern s32 g_hud_value_tertiary;

/* HUD ordering-table priority. The sort calls read it as a 16-bit value (lhu);
 * its three setters store a whole 32-bit value. */
extern s32 g_hud_ot_priority;

void hud_value_draw(GsOT* ot);

/* pictures */
/* Currently resident picture set, reset to -1 by map_state_init. */
extern s32 g_picture_loaded_set;
extern s32 g_picture_background_loaded_set;

/* Destination image buffer (read back as a TIM by sound_novel_opcode_0a_load_picture). */
extern u32* g_picture_buffer;

void picture_load_background_set(s32 set);
void picture_load_set(s32 value);

/* tiles */
extern s32* g_world_map_tile_descriptors;

void world_map_load_texture(void);
void world_map_texture_stream_update(file_load_descriptor_t* stream);
void world_map_tile_atlas_init(void);
void world_map_project_tiles(void);
void world_map_tiles_draw(GsOT* ot);

/* camera */
/* Map camera view over adjacent origin, scale and rotation vectors. Projection
 * addresses origin and scale from rotation; pan and zoom update the VECTOR fields. */
typedef struct world_map_projection_view {
    VECTOR origin;    /* 0x00 */
    VECTOR scale;     /* 0x10 */
    SVECTOR rotation; /* 0x20 */
} world_map_projection_view_t;
typedef char world_map_projection_view_size_must_be_40[sizeof(world_map_projection_view_t) == 40 ? 1 : -1];

/* Values of world_map_pan_t.flags. */
typedef enum world_map_pan_flag {
    WORLD_MAP_PAN_ACTIVE = 0x01,
    WORLD_MAP_PAN_ZOOMED_OUT = 0x02 /* g_world_map_zoom: the wide view, or the move to it */
} world_map_pan_flag_e;

/* Wrapped projection move: the origin and final target are copied as VECTORs. */
typedef struct world_map_pan {
    s32 flags; /* 0x00; world_map_pan_flag_e */
    s32 distance;
    s32 progress;
    VECTOR origin;
    VECTOR target;
} world_map_pan_t;
typedef char world_map_pan_size_must_be_44[sizeof(world_map_pan_t) == 44 ? 1 : -1];

/* The zoom between the normal and the wide view (g_world_map_zoom). */
typedef struct world_map_zoom {
    s32 flags;
    s32 frame;
    s32 duration;
    VECTOR origin;
} world_map_zoom_t;
typedef char world_map_zoom_size_must_be_28[sizeof(world_map_zoom_t) == 28 ? 1 : -1];

extern world_map_projection_view_t g_world_map_projection_view;
extern world_map_pan_t g_world_map_pan;

/* Provisional: flags bit 0 runs the zoom, bit 1 selects the zoomed-out direction. */
extern world_map_zoom_t g_world_map_zoom;

void world_map_marker_update(void);
s32 world_map_pan_update(void);
s32 world_map_pan_start(const point32_t* point, const SVECTOR* coordinates);
s32 world_map_point_needs_pan(const point32_t* point);

/* scrolling */
/* The range of the map scroll position. */
typedef enum world_map_scroll_limits {
    WORLD_MAP_SCROLL_MIN_X = -116,
    WORLD_MAP_SCROLL_MAX_X = 128,
    WORLD_MAP_SCROLL_MIN_Y = -64,
    WORLD_MAP_SCROLL_MAX_Y = 80
} world_map_scroll_limits_e;

/* One axis of accelerating world-map scroll. Value, limit, step and decay
 * use Q8; output is the whole-pixel speed. */
typedef struct world_map_scroll_axis {
    s32 value;
    s32 limit;
    s32 step;
    s32 decay;
    s32 output; /* 0x10; whole-pixel speed with direction-dependent rounding */
} world_map_scroll_axis_t;

void world_map_scroll_axis_update(world_map_scroll_axis_t* axis, s32 direction);
void world_map_scroll_apply(point32_t* amount, VECTOR* position);
void world_map_horizontal_scroll_apply(s32* amount, s32* position);
void world_map_vertical_scroll_apply(point32_t* amount, VECTOR* position);

/* map dots */
/* Static location dots select their appearance by category and subtype. */
typedef struct world_map_dot {
    s32 kind;
    s32 sub_kind;
    SVECTOR position; /* 0x08; world-map position, RotTrans input */
    /* 0x10; enqueued whole for the animated display-object renderer. */
    display_object_animated_t render;
} world_map_dot_t;
typedef char world_map_dot_size_must_be_52[sizeof(world_map_dot_t) == 52 ? 1 : -1];

/* world_map_marker_state_t.flags: work pending for
 * world_map_marker_update, and the marker's sprite set. */
typedef enum world_map_marker_flag {
    WORLD_MAP_MARKER_UPDATE_POSITION = 0x01,
    WORLD_MAP_MARKER_UPDATE_DIRECTION = 0x02,
    WORLD_MAP_MARKER_ALTERNATE_DIRECTION_SET = 0x04 /* set while the marker travels a route */
} world_map_marker_flag_e;

/* The player marker stores location and picture IDs, unlike static dot categories.
 * Its palette is 0, 2 or 3, selected by EVENT_SCRIPT_VAR_TOWN_BACKGROUND. */
typedef struct world_map_marker {
    s32 location_id;
    s32 picture_id;
    SVECTOR position;
    display_object_animated_t render;
} world_map_marker_t;
typedef char world_map_marker_size_must_be_52[sizeof(world_map_marker_t) == 52 ? 1 : -1];

/* Pending projection/direction changes, heading and the player marker. */
typedef struct world_map_marker_state {
    s32 flags; /* 0x00; world_map_marker_flag_e */
    s32 angle;
    world_map_marker_t marker;
} world_map_marker_state_t;

extern world_map_dot_t g_world_map_dots[];
extern world_map_marker_state_t g_world_map_marker_state;

void world_map_dots_init(void);
void world_map_marker_and_camera_init(void);
void world_map_project_dots(void);
void world_map_pulse_dots(void);
s32 world_map_dot_find(point32_t point);
s32 world_map_dot_snap_get(point32_t point, s32* step_x, s32* step_y);

/* routes */
/* World-map routes: g_world_map_routes and the flags from
 * EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST. */
enum { WORLD_MAP_ROUTE_COUNT = 48 };

/* Values of world_map_path_animation_t.flags. */
typedef enum world_map_path_animation_flag {
    WORLD_MAP_PATH_ANIMATION_RUNNING = 0x01, /* the segment timer runs */
    WORLD_MAP_PATH_ANIMATION_REVERSED = 0x02 /* the route is shown end to start */
} world_map_path_animation_flag_e;

/* The path animation (g_world_map_path_animation): a route drawn segment by segment. route_id is the route
 * (offset by 0x22c, its route-visible script variable); each segment is one two-point step of the route polyline that
 * follows the segment list, drawn over its length / 32 frames, and segment i runs from point 2i to point 2i + 2. */
typedef struct world_map_path_animation {
    u16 flags;              /* 0x00; world_map_path_animation_flag_e */
    s16 route_id;           /* 0x02; route; + 0x22c is its route-visible script variable */
    s16 segment_count;      /* 0x04 */
    s16 segment_index;      /* 0x06 */
    s16 segment_frames[16]; /* 0x08; frames each segment takes to unroll */
    s16 frame_timer;        /* 0x28 */
    s16 point_count;        /* 0x2a */
    s16 coords[48];         /* 0x2c; up to 24 x/y pairs in the reserved tail */
} world_map_path_animation_t;
typedef char world_map_path_animation_size_must_be_140[sizeof(world_map_path_animation_t) == 140 ? 1 : -1];

extern world_map_path_animation_t g_world_map_path_animation;

void world_map_routes_draw(GsOT* ot);
void world_map_path_animation_prepare(s32 from, s32 to);
void world_map_path_animation_draw(GsOT* ot);

/* menu levels */
/* Provisional: the WORLD_MENU level that closes the world map, pushed by world_map_menu_level_open.
 * map_menu_return_update is its only reader: delay counts down the frames the fade needs and pending latches
 * while WORLD_MAP_FLAG_SCREEN_CLEARED is up. */
typedef struct world_map_menu_level {
    s32 delay;
    s32 pending; /* 0x04 */
} world_map_menu_level_t;

/* Provisional: the world-map cursor level (ROOT) stepped by cursor_level_update. */
typedef struct world_map_cursor_level {
    s32 window_index;     /* 0x00; the map cursor window */
    s32 alt_window_index; /* 0x04; the wide-view window swapped in by 0x80 */
    s32 wide_view;        /* 0x08; non-zero while the wide view is shown */
    s32 snap_hit;         /* 0x0c; dot index returned by the snap probe */
    s32 pending_mode;     /* 0x10 */
} world_map_cursor_level_t;

/* The WORLD_MAP_LOCATION_VISIBILITY level pushed by visibility_level_open and stepped by
 * visibility_level_update; only the first four fields are known. */
typedef struct world_map_visibility_level {
    s32 dot_index;     /* 0x00; map dot, and script variable 0x200 + dot_index */
    s32 erase;         /* 0x04; nonzero clears visibility instead of revealing the dot */
    s32 delay;         /* 0x08; frames the step handler waits, 0x10 here */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} world_map_visibility_level_t;
typedef char world_map_visibility_level_size_must_be_92[sizeof(world_map_visibility_level_t) == 92 ? 1 : -1];

/* Provisional: the WORLD_MAP_PATH level pushed by path_level_open, decoded from the four fields it
 * writes. Its step handler path_level_update reads only the last two. The first two are the script's
 * path arguments, which location_script_requests_dispatch passes from g_location_script_state.args. Only
 * the first four fields of the slot are known. */
typedef struct world_map_path_level {
    s32 from_location; /* 0x00; g_location_script_state.args[0] */
    s32 to_location;   /* 0x04; g_location_script_state.args[1] */
    s32 erase;         /* 0x08; step handler walks the path backwards when set */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} world_map_path_level_t;
typedef char world_map_path_level_size_must_be_92[sizeof(world_map_path_level_t) == 92 ? 1 : -1];

/* The text ids of TEXT_ID_WORLD_MENU_BASE that the code names: the bar, the
 * Brave Story, the tutorial and the location menu. The first five entries are
 * the bar's rows, TEXT_ID_WORLD_MENU_BASE + bar_row_e. */
typedef enum world_map_text_id {
    TEXT_ID_OK_NO = 0xb805,                  /* "O K / N O" */
    TEXT_ID_BAR_WELCOME = 0xb806,            /* the bar owner: "Welcome. Come in and relax." */
    TEXT_ID_BAR_GOODBYE = 0xb807,            /* "Thank you! Come again." */
    TEXT_ID_RUMORS_INTRO = 0xb808,           /* "Rumors are floating around." */
    TEXT_ID_RUMOR_ABOUT = 0xb809,            /* "This rumor's about ..." */
    TEXT_ID_NO_RUMORS = 0xb80a,              /* "Haven't heard any rumors yet." */
    TEXT_ID_PROPOSITIONS_INTRO = 0xb80b,     /* "I know a great way to make a buck." */
    TEXT_ID_PROPOSITION_ABOUT = 0xb80c,      /* "This's what I know about ..." */
    TEXT_ID_NO_PROPOSITIONS = 0xb80d,        /* "Haven't heard any offers lately." */
    TEXT_ID_NO_MORE_JOBS = 0xb80e,           /* "I can't give anymore jobs." */
    TEXT_ID_ACCEPT_JOB_PROMPT = 0xb80f,      /* "You want this job?" */
    TEXT_ID_JOB_FEE_PROMPT = 0xb810,         /* "You need ... gil for info etc. Still wanna do it?" */
    TEXT_ID_NOT_ENOUGH_GIL = 0xb811,         /* "You don't got enough gil." */
    TEXT_ID_CHOOSE_UNITS = 0xb812,           /* "So, who you wanna send?" */
    TEXT_ID_CHOOSE_DAYS = 0xb813,            /* "This job's from ... to .... How many days you want?" */
    TEXT_ID_CONFIRM_CONDITIONS = 0xb814,     /* "These are the conditions, OK?" */
    TEXT_ID_JOBS_RETURNED = 0xb819,          /* "Good job. You came back from these jobs." */
    TEXT_ID_HEAR_ABOUT_JOB = 0xb81a,         /* "Wanna hear about ...?" */
    TEXT_ID_JOBS_IN_PROGRESS = 0xb81b,       /* "These jobs're in progress." */
    TEXT_ID_RETURN_TOMORROW = 0xb81c,        /* "I'll have them return tomorrow." */
    TEXT_ID_CALL_BACK_PROMPT = 0xb81d,       /* "What a waste if they quit now. Still wanna call them back?" */
    TEXT_ID_CALL_BACK_ACCEPTED = 0xb81e,     /* "Ok, I'll have them return by tomorrow." */
    TEXT_ID_JOB_CONDITION_LABELS = 0xb821,   /* "Days", "Place", "Unit" with the first two filled in */
    TEXT_ID_CHOOSE_MORE_UNITS = 0xb822,      /* "Who else are you sending?" */
    TEXT_ID_DAYS_LABEL = 0xb823,             /* "Days:" */
    TEXT_ID_DEBUG_VARIABLE_VALUE = 0xb824,   /* the debug list: a script variable and its value */
    TEXT_ID_DEBUG_VARIABLE = 0xb825,         /* the same without the value, for values of 10000 up */
    TEXT_ID_PARTY_NUMBER_LABEL = 0xb826,     /* "Party number:" */
    TEXT_ID_JOB_RESULT_LABELS = 0xb830,      /* "Bonus", "Total", "Job Point" */
    TEXT_ID_FOUR_DIGIT_VALUE = 0xb831,       /* a value, four digits wide */
    TEXT_ID_QUIT_CONTINUE = 0xb83a,          /* "Quit / Continue" */
    TEXT_ID_BRAVE_STORY_ROW_FIRST = 0xb83f,  /* plus brave_story_row_e: "Fact" to "Return" */
    TEXT_ID_INJURED_CASUALTIES = 0xb846,     /* "Injured", "Casualties" and their counts */
    TEXT_ID_BRAVE_STORY_INTRO = 0xb848,      /* Alazlam: "The secrets of the Lion War are uncovered in this book." */
    TEXT_ID_BRAVE_STORY_ENTRY_INFO = 0xb849, /* "This is the information on ..." */
    TEXT_ID_BRAVE_STORY_GOODBYE = 0xb84a,    /* "You must find truth on your own. ..." */
    TEXT_ID_SOUND_NOVEL_RESUME_CHOICES = 0xb84b, /* "This book was canceled earlier. Restart / Read from start" */
    TEXT_ID_SUBSTITUTED_TEXT = 0xb84c,           /* the text of substitution value 0 alone */
    TEXT_ID_JOB_CONTENTS_LABEL = 0xb84e,         /* "Job contents:" */
    TEXT_ID_MONTH_NAME_BASE = 0xb84f,            /* plus the month, 1 to 12: "Jan" to "Dec" */
    TEXT_ID_TOLD_BEFORE = 0xb85c,                /* "Think I told you before but..." */
    TEXT_ID_BAR = 0xb85d,                        /* the four entries of a town's location menu */
    TEXT_ID_SHOP = 0xb85e,
    TEXT_ID_SOLDIER_OFFICE = 0xb85f,
    TEXT_ID_FUR_SHOP = 0xb860,
    TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST
    = 0xb864,                                     /* plus brave_story_proposition_row_e: "Advancing", "Work history" */
    TEXT_ID_VALUE = 0xb866,                       /* a value */
    TEXT_ID_TWO_DIGIT_VALUE = 0xb867,             /* a value, two digits wide */
    TEXT_ID_RAMZA_INFO = 0xb868,                  /* BRAVE_STORY_ENTRY_INFO for person 0: a title and Ramza's name */
    TEXT_ID_RAMZA_TITLE_FIRST = 0xb869,           /* plus his profile: sixteen titles, "Knight" each */
    TEXT_ID_PROPOSITION_HINT_FIRST = 0xb879,      /* plus the proposition: the bar owner's advice on it */
    TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST = 0xb8d9, /* plus tutorial_menu_row_e: "Organization" to "Return" */
    TEXT_ID_TUTORIAL_WELCOME = 0xb8dd,            /* Daravon: "If you know, you can foresee. ..." */
    TEXT_ID_TUTORIAL_PROMPT = 0xb8de,             /* "Know the path to win the battle. Solve your questions here." */
    TEXT_ID_CHOOSE_HUMAN_UNIT = 0xb8e0,           /* "Choose a good human to send, or one with no strings!" */
    TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST = 0xb8ed,    /* plus the floor: "Nogias" to "End" */
    TEXT_ID_TREASURE_FOUND_FIRST = 0xb8f9, /* the eleven messages for a treasure found, to "Treasure Hunter Master" */
    TEXT_ID_LAND_FOUND_FIRST = 0xb904      /* the same for an unexplored land, to "Master Adventurer" */
} world_map_text_id_e;

extern s32 g_world_map_window_index;
extern s32 g_world_map_cursor_window;

void world_map_menus_update(GsOT* context);
void world_map_menu_level_open(void);
void world_map_cursor_level_suspend(menu_stack_record_t* level);

/* screen and event transitions */
/* Provisional: the SCREEN_TRANSITION level pushed by screen_transition_open and stepped by
 * screen_transition_update. */
typedef struct screen_transition_level {
    menu_stack_level_e destination_level; /* 0x00; destination selector, using menu-stack level IDs */
    s32 param_a;                          /* 0x04 */
    s32 param_b;                          /* 0x08 */
    s32 _unused_0c;                       /* 0x0c */
    s32 delay;                            /* 0x10; 0x14 at push */
    s32 phase;                            /* 0x14; cleared at push */
} screen_transition_level_t;
typedef char screen_transition_level_size_must_be_24[sizeof(screen_transition_level_t) == 24 ? 1 : -1];

/* event_transition_level_t.mode, the argument of
 * event_transition_open and the second operand of the
 * world script's load-event instruction. Any value but 0 and 2 acts as 1. */
typedef enum event_transition_mode {
    EVENT_TRANSITION_MODE_SPIN = 0, /* spins the screen in and back out: a random battle */
    /* Spins the screen in with a second sound and whites it out: every
     * battle the world scripts start. */
    EVENT_TRANSITION_MODE_SPIN_IN = 1,
    EVENT_TRANSITION_MODE_FADE = 2 /* a plain screen fade: every scene they start */
} event_transition_mode_e;

/* Values of event_transition_level_t.phase. */
typedef enum event_transition_phase {
    EVENT_TRANSITION_PHASE_SPIN_IN = 0,
    EVENT_TRANSITION_PHASE_SPIN_OUT = 1,
    EVENT_TRANSITION_PHASE_DONE = 2
} event_transition_phase_e;

/* Values of event_transition_level_t.fade_phase. */
typedef enum event_transition_fade {
    EVENT_TRANSITION_FADE_NONE = 0,
    EVENT_TRANSITION_FADE_FLASH = 1,    /* the fill box ramps by 0x10 up to 0x80, then restarts as WHITE_OUT */
    EVENT_TRANSITION_FADE_WHITE_OUT = 2 /* ramps by 8 to white, then leaves the world loop */
} event_transition_fade_e;

/* Values of event_transition_level_t.drift_direction. */
typedef enum event_transition_drift {
    EVENT_TRANSITION_DRIFT_RIGHT = 0x01, /* clear: left */
    EVENT_TRANSITION_DRIFT_DOWN = 0x02   /* clear: up */
} event_transition_drift_e;

/* Provisional: the EVENT_TRANSITION level pushed by event_transition_open and stepped by
 * event_transition_update, which spins or fades the screen out before an event.
 * event_transition_sprite_draw draws g_event_transition_sprite while the parent level's mode
 * is not 2. */
typedef struct event_transition_level {
    s32 frame;           /* 0x00; counts step frames; the sprite updates every third, 1 restores the captured screen */
    s32 drift_direction; /* 0x04; event_transition_drift_e; rand() * 4 >> 15 at push */
    s32 spin_direction;  /* 0x08; rand() * 2 >> 15 at push; selects +/-0x2000 rotation per update */
    s32 phase;           /* 0x0c; event_transition_phase_e */
    s32 fade_phase;      /* 0x10; event_transition_fade_e */
    s32 anim_step;       /* 0x14; +2/+4 per update; drives scale, shade, sounds 0x6c/0x6d and phase changes */
    u8 _unused_18[8];
    s32 mode; /* 0x20; event_transition_mode_e; argument of the push */
} event_transition_level_t;

void screen_transition_open(menu_stack_level_e destination_level, s32 param_a, s32 param_b);
void screen_transition_update(menu_stack_record_t* level);
void event_transition_open(s32 mode);
void event_transition_update(menu_stack_record_t* level);
void event_transition_sprite_draw(GsOT* ot);
void event_transition_seek_battle_bin(void);

/* world map menu (WORLD) */
void world_map_confirm_destination(void);
void world_map_menu_text_window_thread(void);
void world_map_destination_list_thread(void);

#endif

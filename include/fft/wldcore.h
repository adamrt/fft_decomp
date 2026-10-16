#ifndef WLDCORE_H
#define WLDCORE_H

/* WLDCORE.BIN (the world map, bar, propositions and Brave Story): the types and declarations its topic headers share.
 */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* calendar */

extern s32 g_wldcore_calendar_display_x;
extern s32 g_wldcore_calendar_display_y;

/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern calendar_date_t g_wldcore_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];
void wldcore_calendar_advance_day(void);
void wldcore_calendar_convert_date_to_zodiac(s32* month, s32* day);
void wldcore_calendar_advance_days(s32 days);
void wldcore_calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day);

/* file */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream);
void wldcore_file_seek_to_battle_bin(void);

void wldcore_load_scratch_data_blocking(void);
/* input */
extern u32 g_wldcore_input_new_button_presses;
extern s32 g_wldcore_input_repeat_counter_up;
extern s32 g_wldcore_input_repeat_counter_down;
extern s32 g_wldcore_input_repeat_counter_left;
extern s32 g_wldcore_input_repeat_counter_right;
extern s32 g_wldcore_input_repeat_counter_r1;
extern s32 g_wldcore_input_repeat_counter_l1;
extern s32 g_wldcore_input_repeat_initial_delay;
extern s32 g_wldcore_input_repeat_period;
extern s32 g_wldcore_input_secondary_repeat_period;
void wldcore_input_init_repeat_state(void);
u32 wldcore_input_update_repeat(u32 buttons);

/* opcode */

/* Scalar view of g_wldcore_active_saved_record.state_flags for opcode
 * handlers that read-modify-write it beside another record store: the member
 * spelling shares one base register where the target uses absolute addresses. */
extern u16 g_wldcore_state_flags;

/* random encounter */
/* One random-encounter tier: the story-progress threshold that selects it,
 * the chance an encounter happens (a roll of 0..99 at or below it), and the
 * high-bit-first mask of which of the entry's eight ENTDs may be drawn. */
typedef struct wldcore_encounter_tier {
    u8 progress_threshold; /* 0x00 */
    u8 encounter_chance;   /* 0x01 */
    u8 encounter_mask;     /* 0x02 */
} wldcore_encounter_tier_t;

/* One road into a world-map location. Three per location, 19 locations
 * (ids 24..42), reached through the pointer cell g_wldcore_random_battle_data. */
typedef struct wldcore_encounter_entry {
    u8 route;                          /* 0x00 */
    u8 deployment_squad_id;            /* 0x01: deployment zone id = this + 0x200 */
    wldcore_encounter_tier_t tiers[4]; /* 0x02 */
    u8 entd_ids[8];                    /* 0x0e */
    u8 battle_map_id;                  /* 0x16 */
    u8 script_variable_id;             /* 0x17 */
} wldcore_encounter_entry_t;

typedef struct wldcore_deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 entd_ids[8];
} wldcore_deep_dungeon_battle_t;

extern wldcore_deep_dungeon_battle_t* g_wldcore_deep_dungeon_battles;

/* route */
enum { WLDCORE_ROUTE_PATH_CAPACITY = 16 };

typedef struct wldcore_route_path {
    s32 routes[WLDCORE_ROUTE_PATH_CAPACITY];
} wldcore_route_path_t;
typedef char wldcore_route_path_size_must_be_64[sizeof(wldcore_route_path_t) == 64 ? 1 : -1];

/* A travel point: packed x/y, a 32-bit heading, and segment distance. */
typedef struct wldcore_route_point {
    u32 position;
    u32 heading;
    s32 distance;
} wldcore_route_point_t;

/* sound */
/* wldcore_audio_music_slots_t.flags. */
typedef enum wldcore_audio_music_flag {
    WLDCORE_AUDIO_MUSIC_PLAYING = 0x01, /* the current slot's music is audible: a new start fades it out first */
} wldcore_audio_music_flag_e;

/* Playback flags followed by the loaded music IDs for slots 1 and 2. */
typedef struct wldcore_audio_music_slots {
    s32 flags; /* wldcore_audio_music_flag_e */
    s32 tracks[2];
} wldcore_audio_music_slots_t;

/* Value of WLDCORE_AUDIO_COMMAND_PLAY_MUSIC and of
 * wldcore_sound_enqueue_music_start: a music slot (1 or 2) and the
 * main_music_e id to load into it. */
#define WLDCORE_AUDIO_MUSIC(slot, music_id) (((slot) << 8) | (music_id))

/* Commands of the audio queue (wldcore_sound_enqueue_audio_command), and the
 * wait states wldcore_sound_process_audio_queue moves the current command
 * through. */
typedef enum wldcore_audio_command {
    WLDCORE_AUDIO_COMMAND_NONE = 0,       /* idle: takes the next queued command */
    WLDCORE_AUDIO_COMMAND_PLAY_MUSIC = 1, /* value: WLDCORE_AUDIO_MUSIC(slot, music id) */
    WLDCORE_AUDIO_COMMAND_FADE_OUT = 2,   /* value << 2: fade time */
    WLDCORE_AUDIO_COMMAND_FADE_IN = 3,    /* the current slot, to the stored volume over value << 2 */
    WLDCORE_AUDIO_COMMAND_UNLOAD = 4,     /* value: slot, 0 for the current one */
    WLDCORE_AUDIO_COMMAND_WAIT_LOAD = 16,
    WLDCORE_AUDIO_COMMAND_WAIT_FADE_OUT = 17,
    WLDCORE_AUDIO_COMMAND_WAIT_FADE_IN = 18,
} wldcore_audio_command_e;

enum { WLDCORE_AUDIO_QUEUE_CAPACITY = 16 };

/* Audio command FIFO at 0x800d45a4, filled by
 * wldcore_sound_enqueue_audio_command and serviced by wldcore_sound_process_audio_queue, followed
 * by the music state that consumer owns. current_command and count are the
 * 32-bit busy fields the queue-drain waits test. */
typedef struct wldcore_audio_queue {
    wldcore_audio_command_e commands[WLDCORE_AUDIO_QUEUE_CAPACITY];
    s32 values[WLDCORE_AUDIO_QUEUE_CAPACITY];
    wldcore_audio_command_e current_command; /* 0x80 */
    s32 current_value;                       /* 0x84 */
    s32 count;                               /* 0x88 */
    wldcore_audio_music_slots_t music;       /* 0x8c */
    s32 current_slot;                        /* 0x98 */
    s32 volume;                              /* 0x9c */
} wldcore_audio_queue_t;
typedef char wldcore_audio_queue_size_must_be_160[sizeof(wldcore_audio_queue_t) == 160 ? 1 : -1];

/* Sound resource the sound-novel menu passes to suzuki_sound_append_resource. */
extern struct suzuki_sound_resource* g_wldcore_default_sound_novel_sound_resource;
extern wldcore_audio_queue_t g_wldcore_audio_queue;

void wldcore_load_sound_novel_files(s32 chapter);

/* story */
/* wldcore_slideshow_t.flags. */
typedef enum wldcore_slideshow_flag {
    WLDCORE_SLIDESHOW_RUNNING = 0x01,  /* the page timer runs */
    WLDCORE_SLIDESHOW_REVERSED = 0x02, /* the route is shown end to start */
} wldcore_slideshow_flag_e;

/* Provisional: the slideshow page list at 0x800d3bbc. A route travel reuses
 * the record: the route id is the script variable (offset by 0x22c, the
 * route-visible variables), each page is one two-point segment of the route
 * polyline that follows the page list, shown for its length / 32 frames, and
 * page i runs from point 2i to point 2i + 2. */
typedef struct wldcore_slideshow {
    u16 flags;           /* 0x00; wldcore_slideshow_flag_e */
    s16 script_variable; /* 0x02; script variable base, offset by 0x22c */
    s16 page_count;      /* 0x04 */
    s16 page_index;      /* 0x06 */
    s16 page_frames[16]; /* 0x08; frames each page stays up */
    s16 frame_timer;     /* 0x28 */
    s16 point_count;     /* 0x2a */
    s16 coords[1];       /* 0x2c; packed (x, y) pairs */
} wldcore_slideshow_t;

extern s32* g_wldcore_rumor_location_masks;

/* system */
/* g_main_system_flags as wldcore_process_frame_gate last saw it (main_system_flags_e). */
extern u32 g_wldcore_previous_system_flags;

void wldcore_restore_previous_stack(void);
void wldcore_init_stub(void);
void wldcore_reset_game_if_special_keycode_is_pressed(void);

void wldcore_switch_to_stack(void*);
s32 wldcore_entrypoint(void);
void wldcore_init_core(void);
/* thread */
/* Slots that the WLDCORE screens use for threads of their own. */
typedef enum wldcore_thread_slot {
    WLDCORE_THREAD_SLOT_TRANSITION = 4,    /* the zoom thread of a screen transition */
    WLDCORE_THREAD_SLOT_WINDOW_PANEL = 12, /* a list or number panel */
    WLDCORE_THREAD_SLOT_MESSAGE = 14,      /* the message box and script text */
} wldcore_thread_slot_e;

/* Parameter blocks passed to WORLD's status banner, numeric editor and
 * equipment/ability panel threads. */
extern menu_status_thread_t g_wldcore_thread8_params;
extern menu_status_thread_t g_wldcore_thread9_params;
extern menu_status_thread_t g_wldcore_threadc_params;

/* unit */
extern unit_stats_t g_wldcore_status_unit;
party_unit_t* wldcore_get_party_unit(s32 party_index);
void wldcore_unit_build_status_panel_data(s32 party_index);

void wldcore_load_formation_unit_name_and_face(void);
/* gfx */
/* g_wldcore_context_value_display_mode: the layout of the HUD numeric readout
 * (wldcore_gfx_draw_context_value_display). */
typedef enum wldcore_value_display_mode {
    WLDCORE_VALUE_DISPLAY_PLAIN = 0,          /* eight digits */
    WLDCORE_VALUE_DISPLAY_FUNDS = 1,          /* labeled and underlined; the war funds */
    WLDCORE_VALUE_DISPLAY_PLAY_TIME = 2,      /* hours, minutes and seconds */
    WLDCORE_VALUE_DISPLAY_LABELED = 3,        /* PLAIN under a small label */
    WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS = 4, /* a labeled five-digit secondary value, then FUNDS */
} wldcore_value_display_mode_e;

/* g_wldcore_screen_fade_state.flags[]: 32-bit flags[0] controls the screen fade and its fill
 * box; flags[1] controls a second fill box. wldcore_fade_start_screen takes OUT,
 * REDRAW_MAP, STOP_MUSIC and START_MUSIC and adds RUNNING. */
typedef enum wldcore_screen_fade_flag {
    WLDCORE_SCREEN_FADE_RUNNING = 0x01,
    WLDCORE_SCREEN_FADE_OUT = 0x02,         /* fades to black; stays set while the screen is dark */
    WLDCORE_SCREEN_FADE_REDRAW_MAP = 0x04,  /* every fade step sets system flag 0x02 */
    WLDCORE_SCREEN_FADE_BOX_VISIBLE = 0x08, /* the fill box is drawn without a running fade */
    WLDCORE_SCREEN_FADE_STOP_MUSIC = 0x10,  /* also fades the music out and unloads it */
    WLDCORE_SCREEN_FADE_START_MUSIC = 0x20, /* also loads track 0x11b and fades it in */
} wldcore_screen_fade_flag_e;

/* The three parallel arrays share one object, which is why the target builds
 * all three walk pointers from a single symbol address. */
typedef struct wldcore_sortbox_state {
    u32 flags[2];      /* 0x00; wldcore_screen_fade_flag_e */
    s32 priorities[2]; /* 0x08; GsSortBoxFill OT priority of each box, read as a 16-bit value */
    s32 elapsed;       /* 0x10; fade frames run, stepped by wldcore_fade_step_screen */
    s32 duration;      /* 0x14 */
    GsBOXF boxes[2];   /* 0x18; full-screen fill boxes drawn by wldcore_fade_draw_screen_overlays */
} wldcore_sortbox_state_t;
typedef char wldcore_sortbox_state_size_must_be_56[sizeof(wldcore_sortbox_state_t) == 56 ? 1 : -1];

/* libgs GsOT descriptor: the 20-byte ordering-table records at 0x800bb364
 * (main) and 0x800bb3c4 (auxiliary), one per display buffer. The 0x14 stride
 * is what wldcore_init_core indexes these globals with. */
/* The ordering-table handle is libgs GsOT; see include/psx/libgs.h. */
/* Heterogeneous display-object queues: each object's leading 32-bit flags
 * selects the handler in window_dispatch_record_list_variant:
 * bit 0 an animated object (world_animated_render_record_t), bit 1 a sprite
 * (wldcore_window_render_record_t). */
struct wldcore_window_render_record;

extern s32 g_wldcore_picture_sector_offsets[];
extern s32 g_wldcore_background_sector_offsets[];

extern s32 g_wldcore_context_value_display_mode;
extern world_sprite_animation_sequence_t** g_wldcore_anim_sequence_table;
extern world_sprite_cel_header_t** g_wldcore_anim_cel_table;
/* Color of the context-value display, tinted by wldcore_gfx_copy_color_with_tint. */
extern CVECTOR g_wldcore_hud_color;

/* HUD ordering-table priority. The sort calls read it as a 16-bit value (lhu);
 * its three setters store a whole 32-bit value. */
extern s32 g_wldcore_hud_ot_priority;

extern u16* g_wldcore_dissolve_band_order;
extern u8 g_wldcore_dissolve_band_progress[16];

/* Destination image buffer (read back as a TIM by wldcore_opcode_load_picture). */
extern u32* g_wldcore_picture_buffer;
extern u32* g_wldcore_scratch_buffer;

/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern rgb24_t g_wldcore_screen_brightness_rgb;
extern wldcore_sortbox_state_t g_wldcore_screen_fade_state;

void wldcore_clear_screen_and_set_map_clip_rect(s32 mode);

/* location */
/* Provisional: four-byte per-location records reached through g_wldcore_location_records. */
typedef struct wldcore_state_record {
    u8 _unused_00[2];
    u8 picture; /* 0x02; proposition picture + 1, 0 for none */
    u8 is_town; /* 0x03; 1 offers the fixed town entries 0xb85d-0xb85f in the location menu */
} wldcore_state_record_t;

/* Provisional: the type-4 location list level as wldcore_menu_push_location_menu_level
 * fills it. */
typedef struct wldcore_location_list_level {
    s32 window_index;  /* 0x00 */
    s32 cursor_window; /* 0x04; -1 until the cursor record is appended */
    s32 frame_render;  /* 0x08; -1 when there is no picture */
    s32 label_window;  /* 0x0c; -1 when there is no picture */
    s32 extra_render;  /* 0x10; -1 at push; released on close when set (wldcore_menu_slot_set_t.render_b) */
    s32 delay; /* 0x14; 8 at push, counted down by the location step (wldcore_menu_location_menu_level_t.delay) */
    s32 mode;  /* 0x18; 0 then 8 */
    s32 selected_entry;
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34 */
} wldcore_location_list_level_t;

extern u8 g_wldcore_choice_hidden_variable_offsets[];

/* save */
/* Section byte offsets at the start of a loaded sound-novel script. */
typedef struct wldcore_sound_novel_script_file {
    u32 _unknown_00;
    u32 offsets[0]; /* 0x04; variable-length section byte-offset table */
} wldcore_sound_novel_script_file_t;

extern wldcore_saved_record_t g_wldcore_active_saved_record;
void wldcore_reset_selected_saved_record(s32 unused);

/* script */
/* The world-conditional interpreter clears this 32-bit result before each
 * block. Comparisons set CONDITION_FAILED when their predicate rejects the
 * block. Action opcodes set ACTION_REACHED to end the block and set an action
 * bit when a request is staged. */
typedef enum wldcore_script_result_flags {
    WLDCORE_SCRIPT_RESULT_ACTION_REACHED = 0x001,
    WLDCORE_SCRIPT_RESULT_CONDITION_FAILED = 0x002,
} wldcore_script_result_flags_e;

typedef enum wldcore_script_action_flags {
    WLDCORE_SCRIPT_ACTION_CHOICE = 0x004,
    WLDCORE_SCRIPT_ACTION_LOAD_EVENT = 0x008,
    WLDCORE_SCRIPT_ACTION_TEXT = 0x010,
    WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY = 0x020,
    WLDCORE_SCRIPT_ACTION_SET_VAR_2 = 0x040,
    WLDCORE_SCRIPT_ACTION_DRAW_PATH = 0x080,
    WLDCORE_SCRIPT_ACTION_ERASE_PATH = 0x100,
    WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION = 0x200,
    WLDCORE_SCRIPT_ACTION_DRAW_LOCATION = 0x400,
    WLDCORE_SCRIPT_ACTION_ERASE_LOCATION = 0x800,
    WLDCORE_SCRIPT_ACTION_PATH_MASK = WLDCORE_SCRIPT_ACTION_DRAW_PATH | WLDCORE_SCRIPT_ACTION_ERASE_PATH,
    WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK
    = WLDCORE_SCRIPT_ACTION_DRAW_LOCATION | WLDCORE_SCRIPT_ACTION_ERASE_LOCATION,
    WLDCORE_SCRIPT_ACTION_MAP_MENU_MASK = WLDCORE_SCRIPT_ACTION_PATH_MASK | WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION
        | WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK,
} wldcore_script_action_flags_e;

extern u16* g_wldcore_default_script_base;

/* text */

/* Provisional: owner of a text-list render record, as
 * wldcore_list_build_tutorial_category_panel_image reads it; only the record
 * index, row count and text-id rows are established. */
typedef struct wldcore_text_list_panel {
    u8 _unused_00[8];
    s32 render_index; /* 0x08 */
    u8 _unused_0c[0x24];
    s32 row_count;   /* 0x30 */
    s32 text_ids[1]; /* 0x34; row_count entries, relative to text 0xb8d9 */
} wldcore_text_list_panel_t;

/* Blocks of the message file (wldcore_load_message_block_if_changed). */
typedef enum wldcore_message_block {
    WLDCORE_MESSAGE_BLOCK_RUMORS = 0,
    WLDCORE_MESSAGE_BLOCK_PROPOSITIONS = 1,
    WLDCORE_MESSAGE_BLOCK_UNEXPLORED_LANDS = 2,
    WLDCORE_MESSAGE_BLOCK_TREASURES = 3,
    WLDCORE_MESSAGE_BLOCK_STORY_EVENTS = 4,
    WLDCORE_MESSAGE_BLOCK_TUTORIAL_TOPICS = 5,
    WLDCORE_MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS = 10,
    WLDCORE_MESSAGE_BLOCK_SCRIPT_FLAGS_01A4_01BB = 11,
    WLDCORE_MESSAGE_BLOCK_PROPOSITION_REPORT_FIRST = 16, /* one block per proposition id */
} wldcore_message_block_e;

/* State for a paged text window and its four decorative window records. */
typedef struct wldcore_text_scrollable_window {
    s32 image_x; /* 0x00; text image position.x for wldcore_window_build_render_record_image; cleared by the setups */
    s32 image_y; /* 0x04; text image position.y; 0x70 or 0x60 from the setups */
    point32_t base;
    s32 text_width;
    s32 rows_per_page;
    s32 measured_columns;
    s32 measured_rows;
    /* 0x20..0x2e: copy of the optional extra render record's base and bounds, taken as two block
     * copies; 0x800906c0 scrolls base.y by row * 16 into the record's base_y and clips the record's
     * y and height from the bounds. */
    point32_t extra_base; /* 0x20 */
    RECT extra_bounds;    /* 0x28 */
    s32 scroll_state;     /* 0x30; wldcore_text_scroll_state_e */
    s32 text_id;
    s32 left_window_index;
    s32 middle_window_index;
    s32 right_window_index;
    s32 render_record_index;
    s32 lower_window_index;
    s32 priority; /* 0x4c; OT priority of the text window's records (wldcore_window_init_record) */
    s32 last_page_row;
    s32 page_start_row;
    s32 scroll_row;            /* 0x58; row shown while 0x800903e4 scrolls (scroll_state 1/2) */
    s32 image_coordinate_mode; /* 0x5c; coordinate mode of the text render record image */
    s32 scroll_timer;          /* 0x60; frame counter of the auto-scroll state */
    s32 extra_render_index;    /* 0x64; optional render record scrolled with the text, -1 if none */
    s32 text_substitutions[2]; /* 0x68; copied to the text substitution values unless -1 */
} wldcore_text_scrollable_window_t;
typedef char wldcore_text_scrollable_window_size_must_be_112[sizeof(wldcore_text_scrollable_window_t) == 112 ? 1 : -1];

extern s32 g_wldcore_message_block_sector_offsets[];
extern wldcore_text_scrollable_window_t g_wldcore_scrollable_text_window;
extern u8 g_wldcore_message_buffer[];
void wldcore_load_message_block_if_changed(s32 block);

/* proposition */
/* Provisional: the job-report confirmation level. The cursor window at +0x00
 * is moved between the two rows of the render record at +0x04, and +0x08
 * records which of the two rows is selected.
 * wldcore_list_handle_completed_propositions_input pushes this level (type
 * 0x0f) over the yes/no panel and stores the argument at +0x10. */
typedef struct wldcore_proposition_report_confirm_level {
    s32 cursor_window; /* 0x00; window record moved between the two rows */
    s32 render_index;  /* 0x04; render record holding the confirmation text */
    s32 selection;     /* 0x08; 0 = upper row, 1 = lower row */
    s32 _unused_0c;
    s32 argument; /* 0x10; argument of the level wldcore_menu_push_proposition_report_level pushes */
} wldcore_proposition_report_confirm_level_t;

/* wldcore_menu_send_unit_level_t.mode. */
typedef enum wldcore_send_unit_mode {
    WLDCORE_SEND_UNIT_MODE_SELECT = 0,
    WLDCORE_SEND_UNIT_MODE_CANCEL = 1,       /* cross: closes back to the available propositions */
    WLDCORE_SEND_UNIT_MODE_REPLACE = 2,      /* three are chosen: picks the slot the unit replaces */
    WLDCORE_SEND_UNIT_MODE_STATUS_OPEN = 3,  /* square: waits for the list threads, then opens the status panels */
    WLDCORE_SEND_UNIT_MODE_STATUS = 4,       /* unit status, browsed with L1 and R1 */
    WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE = 5, /* waits for the status threads, then reopens the list */
    WLDCORE_SEND_UNIT_MODE_ACCEPT = 6,       /* start: closes and moves on to the day count */
} wldcore_send_unit_mode_e;

/* Provisional: the type-0x10 proposition report level pushed by
 * wldcore_menu_push_proposition_report_level (0x800779d0), which stores its argument at
 * +0x2c and starts WORLD thread 14, and stepped by
 * wldcore_proposition_step_report_level. */
typedef struct wldcore_proposition_report_level {
    s32 state; /* 0x00; step index, -1 .. 0x12; pushed as -1, or 0x63 when the proposition was recalled */
    s32 phase; /* 0x04; wldcore_proposition_step_message phase; cleared at push */
    u8 _unused_08[4];
    s32 load_phase;   /* 0x0c; wldcore_proposition_report_load_e; MESSAGES at push */
    s32 render_index; /* 0x10; picture render record; -1 at push */
    u8 _unused_14[0x18];
    s32 proposition; /* 0x2c; argument of the push */
} wldcore_proposition_report_level_t;

/* Provisional: proposition-result menu level (type 0x11) pushed by
 * wldcore_proposition_push_result_level and stepped by 0x80078bb8. */
typedef struct wldcore_proposition_result_level {
    s32 render_index; /* 0x00 */
    u8 _unused_04[0xc];
    s32 phase; /* 0x10; wldcore_proposition_result_phase_e */
    u8 _unused_14[0x14];
    s32 delay;       /* 0x28; frames before the success jingle restarts */
    s32 proposition; /* 0x2c; index into g_main_active_propositions */
    u8 _unused_30[0x2c];
} wldcore_proposition_result_level_t;

/* One 23-byte proposition row expanded to named 16-bit fields by
 * wldcore_unpack_proposition_row. */
typedef struct wldcore_proposition_fields {
    u16 category_index;  /* 0x00; 1-based row in proposition data records 3, 10 and 11 */
    u16 job_list_index;  /* 0x02 */
    u16 id;              /* 0x04 */
    u16 _unused_06;      /* 0x06 */
    u16 fee_a;           /* 0x08; remapped through data record 1 */
    u16 fee_b;           /* 0x0a; remapped through data record 1 */
    u16 min_days;        /* 0x0c */
    u16 max_days;        /* 0x0e */
    u16 name_text;       /* 0x10; 1-based errand name, text 0xf7ff + n */
    u16 jp_reward;       /* 0x12 */
    u16 discovery_kind;  /* 0x14; wldcore_proposition_reward_e TREASURE or UNEXPLORED_LAND, else see random_bonus */
    u16 random_bonus;    /* 0x16; 1 enables the three-tier random reward */
    u16 gil_reward;      /* 0x18 */
    u16 _unused_1a[3];   /* 0x1a */
    u16 location;        /* 0x20; 0 for any location */
    u16 condition_flags; /* 0x22; WLDCORE_PROPOSITION_CONDITION_* */
    u16 _unused_24;      /* 0x24 */
    u16 speaker_mode;    /* 0x26; wldcore_proposition_speaker_e */
    u16 preferred_job;   /* 0x28 */
    u16 required_script_var_6f; /* 0x2a */
    u16 condition_value;        /* 0x2c; zodiac month or proposition status index */
} wldcore_proposition_fields_t;

typedef char wldcore_proposition_fields_size_must_be_46[sizeof(wldcore_proposition_fields_t) == 46 ? 1 : -1];

/* Window and render handles passed together to the palette helper. */
typedef struct wldcore_window_render_pair {
    s32 window_index;
    s32 render_index;
} wldcore_window_render_pair_t;

/* Provisional: proposition send-unit menu level. The handler at 0x80075bd8
 * selects one of three party slots and owns its portrait render records. */
typedef struct wldcore_menu_send_unit_level {
    wldcore_window_render_pair_t list_frame; /* 0x00 */
    s32 frame_window_a;                      /* 0x08 */
    s32 frame_window_b;                      /* 0x0c */
    s32 preview_render;                      /* 0x10 */
    s32 list_render;                         /* 0x14 */
    s32 cursor_window;                       /* 0x18 */
    u8 _unused_1c[4];
    s32 mode;          /* 0x20; wldcore_send_unit_mode_e */
    s32 shown_preview; /* 0x24 */
    s32 slot;          /* 0x28 */
    u8 _unused_2c[4];
    s32 unit_count;      /* 0x30; wrap bound, written by wldcore_proposition_load_send_unit_candidates */
    s32 portrait_render; /* 0x34 */
    s32 portrait_frame;  /* 0x38 */
    s32 portrait_window; /* 0x3c */
    s32 left_arrow;      /* 0x40 */
    s32 right_arrow;     /* 0x44 */
    u8 _unused_48[0x10];
    s32 shown_unit; /* 0x58 */
} wldcore_menu_send_unit_level_t;
typedef char wldcore_menu_send_unit_level_size_must_be_92[sizeof(wldcore_menu_send_unit_level_t) == 92 ? 1 : -1];

/* Saved proposition-list cursor row. Declared as an array: GCC's alias
 * heuristic needs an in-struct object to keep the target's load order in
 * wldcore_list_handle_active_propositions_input; a scalar does not. */
extern s32 g_wldcore_active_propositions_saved_cursor[];

extern s32 g_wldcore_participant_list_saved_cursor;
extern wldcore_proposition_fields_t g_wldcore_selected_proposition_row[];

void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index);

s32 wldcore_get_completion_milestone_rank(void);
/* scroll */
typedef enum wldcore_scroll_limits {
    WLDCORE_SCROLL_MIN_X = -116,
    WLDCORE_SCROLL_MAX_X = 128,
    WLDCORE_SCROLL_MIN_Y = -64,
    WLDCORE_SCROLL_MAX_Y = 80
} wldcore_map_scroll_limits_e;

/* bar */
/* Panel menu levels: number panels (0x8006f294, 0x80073ef8) and list panels
 * (0x8007dfcc, 0x80084230, 0x80071aa0). */
typedef struct wldcore_menu_panel_level {
    /* 0x00; number panel: 0 running, 1 confirmed, 2 canceled; list panels
     * also use -1 for cancellation. Read after WORLD thread 12 ends. */
    s32 result;
    s32 variable_id; /* 0x04; script variable edited by number panels */
    u8 _unused_08[0x28];
    s32 entry_count; /* 0x30; rows in the panel list */
    /* 0x34; entry text ids, filled by wldcore_map_build_location_menu_entries (0x8008d2c8), whose result is
     * the entry count; the same offset as the list_window and proposition_list entries */
    s32 entries[10];
} wldcore_menu_panel_level_t;

/* Provisional: list-window level beneath the flag-list panels; 0x80087b94
 * and 0x8008343c hide its content render record (flag 0x10) when they push.
 * 0x800928ac clears flag 0x10 on the upper and lower windows when it pops
 * back to it. */
typedef struct wldcore_menu_list_window_level {
    s32 main_window;
    s32 side_window;    /* 0x04 */
    s32 frame_render;   /* 0x08 */
    s32 content_render; /* 0x0c */
    u8 _unused_10[8];
    s32 mode;           /* 0x18; wldcore_list_window_mode_e */
    s32 selected_entry; /* 0x1c; initial row from g_wldcore_brave_story_saved_cursor */
    s32 upper_window;   /* 0x20 */
    s32 lower_window;   /* 0x24 */
    u8 _unused_28[8];
    s32 entry_count; /* 0x30; rows drawn by 0x80080e54 */
    s32 entries[10]; /* 0x34; row kinds of the owning list (wldcore_*_row_e); brave story: text id 0xb83f + kind */
} wldcore_menu_list_window_level_t;

/* list */
/* Menu level owning two window records and a render record: 0x800712b0
 * pushes it (type 5); the tutorial text window 0x80084578 hides all three
 * with flag 0x10 and 0x80084748 restores them. */
typedef struct wldcore_menu_window_pair_render_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 panel_rows;    /* 0x0c; cleared by 0x800712b0 */
} wldcore_menu_window_pair_render_level_t;

/* camera */

/* The 52-byte world map dot records at 0x800d3ca8. */
typedef struct wldcore_map_dot {
    s32 kind;
    s32 sub_kind;
    SVECTOR position; /* 0x08; world-map position, RotTrans input */
    /* 0x10; enqueued whole for the animated display-object renderer. Its palette
     * is 0, 2 or 3 for the player's marker, from EVENT_SCRIPT_VAR_TOWN_BACKGROUND. */
    world_animated_render_record_t render;
} wldcore_map_dot_t;
typedef char wldcore_map_dot_size_must_be_52[sizeof(wldcore_map_dot_t) == 52 ? 1 : -1];

/* Zoom transition at 0x800d3c8c, ending before the map-dot array at 0x800d3ca8. */
typedef struct wldcore_zoom_motion {
    s32 flags;
    s32 frame;
    s32 duration;
    VECTOR origin;
} wldcore_zoom_motion_t;
typedef char wldcore_zoom_motion_size_must_be_28[sizeof(wldcore_zoom_motion_t) == 28 ? 1 : -1];

/* Provisional: one axis of the accelerating map scroll stepped by
 * wldcore_map_update_scroll_axis_speed: the fixed-point accumulator, its
 * signed limit, the acceleration and decay steps, and the exported whole-unit
 * speed. */
typedef struct wldcore_axis_state {
    s32 value;
    s32 limit;
    s32 step;
    s32 decay;
    s32 output; /* 0x10; value >> 8, truncated toward zero */
} wldcore_axis_state_t;

/* Provisional: the 24-byte projected-tile table at 0x800c7320, filled by
 * wldcore_map_init_tile_atlas and projected by the RotTrans pass. */
typedef struct wldcore_projected_entry {
    s32 flags;    /* 0x00; wldcore_map_tile_flag_e */
    s32 texture;  /* 0x04; texture rectangle packed u << 24 | v << 16 | w << 8 | h, unpacked into the poly UVs */
    s32 map_x;    /* 0x08; tile x (-256 + widths), RotTrans vx */
    s32 map_y;    /* 0x0c; tile y (-192 + heights), RotTrans vy */
    s32 screen_x; /* 0x10; projected x, culled against the 0x121 screen span */
    s32 screen_y; /* 0x14; projected y, culled against 0x111 */
} wldcore_projected_entry_t;
typedef char wldcore_projected_entry_size_must_be_24[sizeof(wldcore_projected_entry_t) == 24 ? 1 : -1];

/* map */
/* The type-0x36 menu level pushed by wldcore_push_map_location_visibility_level
 * and stepped by wldcore_map_step_location_visibility_change; only the first
 * four 32-bit fields are known. */
typedef struct wldcore_map_visibility_level {
    s32 dot_index;     /* 0x00; map dot, and script variable 0x200 + dot_index */
    s32 was_set;       /* 0x04; whether the variable was already set */
    s32 delay;         /* 0x08; frames the step handler waits, 0x10 here */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} wldcore_map_visibility_level_t;
typedef char wldcore_map_visibility_level_size_must_be_92[sizeof(wldcore_map_visibility_level_t) == 92 ? 1 : -1];

/* Provisional: the type-0x35 menu level pushed by
 * wldcore_menu_push_map_path_level, decoded from the four 32-bit values it writes and
 * corroborated by its step handler at
 * 0x8006dbb8, which reads only the last two: +0x08 selects the erase
 * direction and +0x0c is the one-shot sound flag. The two leading 32-bit fields are
 * the script's path arguments, which the caller
 * wldcore_menu_dispatch_pending_script_requests passes as g_wldcore_script_state.args[0]
 * and g_wldcore_script_state.args[1]. The record is one menu-stack slot, so the
 * view is 0x5c bytes wide and only the first four 32-bit fields are known. */
typedef struct wldcore_map_path_level {
    s32 path_id;       /* 0x00; g_wldcore_script_state.args[0] */
    s32 path_id_hi;    /* 0x04; g_wldcore_script_state.args[1] */
    s32 erase;         /* 0x08; step handler walks the path backwards when set */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} wldcore_map_path_level_t;
typedef char wldcore_map_path_level_size_must_be_92[sizeof(wldcore_map_path_level_t) == 92 ? 1 : -1];

/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both 32-bit values as one point32_t. */
extern point32_t g_wldcore_context_value_display_position;

/* Indexed so the store in wldcore_location_process_entry stays behind the
 * window-flag store (MEM_IN_STRUCT_P). */
extern s32 g_wldcore_next_map_id[];

/* menu */
/* Slots of g_wldcore_menu_cursor_states: the remembered row of each list. */
typedef enum wldcore_cursor_slot {
    WLDCORE_CURSOR_SLOT_LOCATION_MENU = 0,
    WLDCORE_CURSOR_SLOT_BAR_MENU = 1,
    WLDCORE_CURSOR_SLOT_RUMORS = 2,
    WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS = 3,
    WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS = 4,
    WLDCORE_CURSOR_SLOT_PROPOSITIONS = 5,
    WLDCORE_CURSOR_SLOT_UNEXPLORED_LANDS = 7,
    WLDCORE_CURSOR_SLOT_TREASURES = 8,
    WLDCORE_CURSOR_SLOT_STORY_EVENTS = 10,
    WLDCORE_CURSOR_SLOT_SET_SCRIPT_VARIABLES_03C0_03FF = 11,
    WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS = 12,
    WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST = 15,
    WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS = 16,
    WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS = 18,
    WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES = 19,
    WLDCORE_CURSOR_SLOT_SCRIPT_FLAGS_01A4_01BB = 20,
} wldcore_cursor_slot_e;

enum { WLDCORE_CURSOR_SLOT_COUNT = 21 };

/* Rows of g_wldcore_anim_sequence_table that the code selects: the animation
 * a window record plays. The CURSOR and *_PRESS sequences other than
 * CURSOR_IDLE end on a pinned frame. */
typedef enum wldcore_anim_sequence_id {
    WLDCORE_ANIM_SEQUENCE_CURSOR_STILL = 0, /* list cursor, one pinned frame */
    WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS = 1, /* list cursor after its row is confirmed */
    WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE = 2,  /* list cursor of the level with focus, looping */
    WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL = 4,
    WLDCORE_ANIM_SEQUENCE_MAP_FRAME = 5,
    WLDCORE_ANIM_SEQUENCE_ROOT_MENU_FRAME = 6,
    WLDCORE_ANIM_SEQUENCE_TEXT_PAGE_PROMPT = 8, /* shown while a paged text window has more pages */
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_UP = 9,
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_DOWN = 10,
    WLDCORE_ANIM_SEQUENCE_LOCATION_PICTURE_BORDER = 11,
    WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER = 12,
    WLDCORE_ANIM_SEQUENCE_MAP_EDGE_ARROWS = 13, /* four arrows at the screen edges, two alternating frames */
    WLDCORE_ANIM_SEQUENCE_MESSAGE_LABEL = 14,
    WLDCORE_ANIM_SEQUENCE_TEXT_SCROLL_THUMB = 15,
    WLDCORE_ANIM_SEQUENCE_MARKER_FIRST = 16,           /* the player's marker, eight facings */
    WLDCORE_ANIM_SEQUENCE_MARKER_ALTERNATE_FIRST = 24, /* the same eight at the faster frame rate */
    WLDCORE_ANIM_SEQUENCE_MONTH_FIRST = 32,            /* twelve month labels */
    WLDCORE_ANIM_SEQUENCE_DIGIT_FIRST = 44,            /* ten date digits */
    WLDCORE_ANIM_SEQUENCE_LOCATION_NAME_FIRST = 55,    /* one label per map dot */
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_TITLE_LABEL = 99,
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL = 100,
    WLDCORE_ANIM_SEQUENCE_PROPOSITION_DAYS_LABEL = 101,
    WLDCORE_ANIM_SEQUENCE_L1_BUTTON = 102,
    WLDCORE_ANIM_SEQUENCE_L1_BUTTON_PRESS = 103,
    WLDCORE_ANIM_SEQUENCE_R1_BUTTON = 104,
    WLDCORE_ANIM_SEQUENCE_R1_BUTTON_PRESS = 105,
    WLDCORE_ANIM_SEQUENCE_MAP_DOT = 106, /* the next row is the dot of a kind-2 location */
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_UPPER = 108,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_LOWER = 109,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON = 110,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON_PRESS = 111,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON = 112,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON_PRESS = 113,
    WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_OVERLAY = 114,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_A = 115,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_B = 116,
    WLDCORE_ANIM_SEQUENCE_SEND_UNIT_PORTRAIT = 117,
    WLDCORE_ANIM_SEQUENCE_LAYOUT_CURSOR_IDLE = 120, /* the layout windows' cursor, looping */
    WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER = 121, /* the brave-story list's two completion markers */
} wldcore_anim_sequence_id_e;

/* Window-layout request at the start of a layout-window menu level. The
 * first three 32-bit fields are what wldcore_window_apply_layout (0x8006f67c) reads. */
typedef struct wldcore_window_layout_request {
    s32 window_index;
    s32 layout; /* 0x04; row of g_wldcore_window_layout_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_wldcore_menu_ordering_table_offset, saved by 0x8006f528 */
} wldcore_window_layout_request_t;

/* Message menu level pushed by 0x8006e860. */
typedef struct wldcore_menu_message_level {
    s32 reload_text;
    s32 saved_blocked_buttons; /* 0x04; g_menu_blocked_buttons at push time */
    s32 window_index;          /* 0x08; window record appended by the type-2 push; cleared by the type-3 push */
    s32 _unknown_0c;           /* 0x0c; set to 8 at push */
} wldcore_menu_message_level_t;

/* Provisional: the type-1 world-map close/return level pushed by
 * wldcore_menu_push_entry_window_level. wldcore_menu_step_return_level
 * (0x8006e208) is the only reader: delay counts down the frames the fade
 * needs and pending latches while system flag 0x04000000 is up. */
typedef struct wldcore_menu_return_level {
    s32 delay;
    s32 pending; /* 0x04 */
} wldcore_menu_return_level_t;

/* Menu level owning two window records; 0x8008b8dc toggles both records' flag 0x10. */
typedef struct wldcore_menu_window_pair_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
} wldcore_menu_window_pair_level_t;

/* wldcore_menu_countdown_t.phase. */
typedef enum wldcore_countdown_phase {
    WLDCORE_COUNTDOWN_PHASE_MESSAGES = 0,     /* circle shows the next message */
    WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE = 1, /* waits for the message box, then starts MAIN_MUSIC_MILESTONE */
    WLDCORE_COUNTDOWN_PHASE_JINGLE = 2,       /* 180 frames, then back to MAIN_MUSIC_BAR */
    WLDCORE_COUNTDOWN_PHASE_CLOSE = 3,        /* waits for the audio queue, then pops */
} wldcore_countdown_phase_e;

/* Provisional: countdown menu object stepped by wldcore_menu_show_next_sequence_message
 * (0x8007a6b8) and driven by 0x8007a86c. The stepper sets phase 1 and clears
 * the timer when the count expires; 0x8007a86c then advances phases 1-3. */
typedef struct wldcore_menu_countdown {
    s32 current;
    s32 remaining;
    s32 phase; /* 0x08; wldcore_countdown_phase_e */
    s32 timer; /* 0x0c; frames spent in phase 2 */
} wldcore_menu_countdown_t;

/* Rumor detail level pushed at 0x80072618. The close handler at 0x80072888
 * reads the saved parent window/render indices at +0x08/+0x0c/+0x10. */
typedef struct wldcore_menu_rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} wldcore_menu_rumor_detail_level_t;

/* Provisional: proposition list level (type 0x26) pushed by 0x80085760: a
 * cursor window and three column-header windows over one render record
 * listing each active proposition's name, participant count and remaining
 * days. */
typedef struct wldcore_menu_proposition_list_level {
    s32 cursor_window; /* 0x00; window sequence 2, moved to the cursor row */
    s32 title_window;  /* 0x04; window sequence 0x63 */
    s32 count_window;  /* 0x08; window sequence 0x64 */
    s32 days_window;   /* 0x0c; window sequence 0x65 */
    s32 render_index;  /* 0x10 */
    u8 _unused_14[8];
    s32 cursor; /* 0x1c; initial row from g_wldcore_active_propositions_saved_cursor */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; indices into g_main_active_propositions */
} wldcore_menu_proposition_list_level_t;

/* Provisional: debug script-variable list level (type 0xb) pushed by
 * 0x8006ea90; 0x8006ed30 draws row_count variables from first_variable
 * into the render record. */
typedef struct wldcore_menu_variable_list_level {
    s32 window_index;   /* 0x00; cursor window */
    s32 render_index;   /* 0x04 */
    s32 first_variable; /* 0x08 */
    s32 cursor;         /* 0x0c; cursor row */
    u8 _unused_10[0x20];
    s32 row_count; /* 0x30 */
} wldcore_menu_variable_list_level_t;

/* Shared detail-panel outputs of menu tags 0x13 and 0x27. */
typedef struct wldcore_menu_proposition_detail_panel {
    u8 _unused_00[8]; /* Leading slots belong to the owning menu variant. */
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
} wldcore_menu_proposition_detail_panel_t;

/* Proposition recall confirmation (menu tag 0x13). The detail-panel builder
 * writes detail_render/detail_rows; the yes/no cursor uses choice_render. */
typedef struct wldcore_menu_proposition_cancel_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 choice_render; /* 0x04 */
    s32 detail_render; /* 0x08 */
    s32 detail_rows;   /* 0x0c */
    s32 proposition;   /* 0x10 */
    s32 choice;        /* 0x14 */
    s32 confirmed;     /* 0x18 */
    u8 _unused_1c[4];
    s32 frame_window; /* 0x20 */
} wldcore_menu_proposition_cancel_confirm_level_t;

/* Proposition participant list (menu tag 0x27). The detail-panel builder
 * fills the render index and row count before the cursor is positioned. */
typedef struct wldcore_menu_participant_list_level {
    s32 cursor_window; /* 0x00 */
    u8 _unused_04[4];
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
    s32 proposition;  /* 0x10 */
    u8 _unused_14[4];
    s32 mode;   /* 0x18; set to WLDCORE_LIST_WINDOW_MODE_BROWSE */
    s32 cursor; /* 0x1c */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
} wldcore_menu_participant_list_level_t;

/* Provisional: proposition participant detail level (type 0x28) pushed by
 * 0x80086f1c with two frame windows; wldcore_unit_build_status_panel_data draws the participant
 * g_main_active_propositions[proposition].participant_indices[participant]. */
typedef struct wldcore_menu_participant_level {
    s32 left_window;  /* 0x00 */
    s32 right_window; /* 0x04 */
    s32 _unknown_08;  /* 0x08; cleared at push */
    s32 _unused_0c;   /* 0x0c; cleared at push */
    s32 proposition;  /* 0x10; index into g_main_active_propositions */
    s32 participant;  /* 0x14; slot in participant_indices */
} wldcore_menu_participant_level_t;

/* wldcore_menu_variable_detail_level_t.phase, shared by the variable,
 * treasure and unexplored-land detail steps. */
typedef enum wldcore_detail_phase {
    WLDCORE_DETAIL_PHASE_SHOWN = 0,
    WLDCORE_DETAIL_PHASE_LOAD = 1,   /* waits for the file */
    WLDCORE_DETAIL_PHASE_BUILD = 2,  /* waits for the message box, then builds the text and picture */
    WLDCORE_DETAIL_PHASE_POP_IN = 3, /* waits for the text window's opening animation */
} wldcore_detail_phase_e;

/* Provisional: script-variable detail level (type 0x20) pushed by
 * 0x8008389c over the list-window level; it keeps the selected entry value
 * (variable index * 16 + value) and the render record it appends. */
typedef struct wldcore_menu_variable_detail_level {
    s32 value;        /* 0x00 */
    s32 render_index; /* 0x04 */
    /* 0x08; no push sets it. The treasure detail stores the saved record of the treasure's sound novel
     * here before it leaves. The treasure and unexplored-land details also read it in their LOAD phase,
     * as a render record index, when it still holds what the last level at this depth left. */
    s32 saved_record_index;
    s32 phase; /* 0x0c; wldcore_detail_phase_e; LOAD at push */
    s32 timer; /* 0x10; cleared at push; text delay or dissolve step, counted per frame */
} wldcore_menu_variable_detail_level_t;

/* wldcore_menu_event_transition_level_t.mode, the argument of
 * wldcore_menu_push_event_transition_level and the second operand of the
 * world script's load-event instruction. Any value but 0 and 2 acts as 1. */

typedef enum wldcore_event_transition_mode {
    WLDCORE_EVENT_TRANSITION_MODE_SPIN = 0, /* spins the screen in and back out: a random battle */
    /* Spins the screen in with a second sound and whites it out: every
     * battle the world scripts start. */
    WLDCORE_EVENT_TRANSITION_MODE_SPIN_IN = 1,
    WLDCORE_EVENT_TRANSITION_MODE_FADE = 2, /* a plain screen fade: every scene they start */
} wldcore_event_transition_mode_e;

/* wldcore_menu_event_transition_level_t.phase. */
typedef enum wldcore_event_transition_phase {
    WLDCORE_EVENT_TRANSITION_PHASE_SPIN_IN = 0,
    WLDCORE_EVENT_TRANSITION_PHASE_SPIN_OUT = 1,
    WLDCORE_EVENT_TRANSITION_PHASE_DONE = 2,
} wldcore_event_transition_phase_e;

/* wldcore_menu_event_transition_level_t.fade_phase. */
typedef enum wldcore_event_transition_fade {
    WLDCORE_EVENT_TRANSITION_FADE_NONE = 0,
    WLDCORE_EVENT_TRANSITION_FADE_FLASH = 1,     /* the fill box ramps by 0x10 up to 0x80, then restarts as WHITE_OUT */
    WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT = 2, /* ramps by 8 to white, then leaves the world loop */
} wldcore_event_transition_fade_e;

/* wldcore_menu_event_transition_level_t.drift_direction. */
typedef enum wldcore_event_transition_drift {
    WLDCORE_EVENT_TRANSITION_DRIFT_RIGHT = 0x01, /* clear: left */
    WLDCORE_EVENT_TRANSITION_DRIFT_DOWN = 0x02,  /* clear: up */
} wldcore_event_transition_drift_e;

/* Provisional: the type-0x23 event-transition level pushed by
 * wldcore_menu_push_event_transition_level (0x8008047c), which stores its
 * argument at +0x20, and stepped by wldcore_menu_step_event_transition_level;
 * wldcore_draw_close_indicator_if_pending draws the indicator sprite at
 * 0x8009f288 while the parent level's mode is not 2. */
typedef struct wldcore_menu_event_transition_level {
    s32 frame;           /* 0x00; counts step frames; the sprite updates every third, 1 restores the captured screen */
    s32 drift_direction; /* 0x04; wldcore_event_transition_drift_e; rand() * 4 >> 15 at push */
    s32 spin_direction;  /* 0x08; rand() * 2 >> 15 at push; selects +/-0x2000 rotation per update */
    s32 phase;           /* 0x0c; wldcore_event_transition_phase_e */
    s32 fade_phase;      /* 0x10; wldcore_event_transition_fade_e */
    s32 anim_step;       /* 0x14; +2/+4 per update; drives scale, shade, sounds 0x6c/0x6d and phase changes */
    u8 _unused_18[8];
    s32 mode; /* 0x20; wldcore_event_transition_mode_e; argument of 0x8008047c */
} wldcore_menu_event_transition_level_t;

/* Type-0x14 sound-novel resume level pushed by
 * wldcore_menu_push_sound_novel_resume_level and stepped by
 * wldcore_menu_step_sound_novel_resume_level: a cursor window over the
 * saved-picture render record and two text render records. Only mode shares
 * its meaning with the type-0x23 level at the same offset. */
typedef struct wldcore_menu_sound_novel_resume_level {
    s32 cursor_window;  /* 0x00; moved between the two option rows */
    s32 picture_render; /* 0x04; VRAM render record of the TIM at g_wldcore_picture_buffer */
    s32 message_render; /* 0x08; text 0xb84c */
    s32 option_render;  /* 0x0c; text 0xb84b; the cursor rows are relative to its base */
    s32 dissolve_step;  /* 0x10; step of the progressive picture upload, 0 once it finishes */
    s32 cursor_row;     /* 0x14; 0 or 1, toggled by up/down; 1 resets the selected saved record */
    u8 _unused_18[8];
    s32 mode; /* 0x20; argument of the push */
} wldcore_menu_sound_novel_resume_level_t;

/* Type-0x33 scrollable text window level pushed by
 * wldcore_menu_push_scrollable_text_window_level and stepped by
 * wldcore_menu_step_scrollable_text_window_level. */
typedef struct wldcore_menu_scrollable_text_level {
    s32 window_index; /* 0x00; window record of the frame */
    u8 _unused_04[0x1c];
    s32 _unknown_20; /* 0x20; 0x28 at push; nothing reads it */
} wldcore_menu_scrollable_text_level_t;

/* Provisional: type-0x31 screen-transition level pushed by
 * wldcore_menu_push_screen_transition_level (0x80088308) and stepped by
 * wldcore_menu_step_screen_transition_level. */
typedef struct wldcore_menu_screen_transition_level {
    s32 screen;     /* 0x00; wldcore_menu_level_type_e of the level to open */
    s32 param_a;    /* 0x04 */
    s32 param_b;    /* 0x08 */
    s32 _unused_0c; /* 0x0c */
    s32 delay;      /* 0x10; 0x14 at push */
    s32 phase;      /* 0x14; cleared at push */
} wldcore_menu_screen_transition_level_t;

/* Provisional: the type-0x16 sound-novel save-confirmation level pushed by
 * wldcore_menu_push_sound_novel_quit_level and stepped by
 * wldcore_menu_step_sound_novel_quit_level. */
typedef struct wldcore_menu_save_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 _unused_0c;
    s32 fade_out;    /* 0x10 */
    s32 fade_in;     /* 0x14 */
    s32 _unknown_18; /* 0x18; cleared at push */
    s32 choice;      /* 0x1c */
} wldcore_menu_save_confirm_level_t;

/* Provisional: the type-0x17 history-text level pushed by
 * wldcore_menu_push_sound_novel_history_level, which stores the newest
 * history text at +0x0c, the three older ones at +0x10..+0x18, phase 1 and
 * step 0. The draw helper consumes this same history record. */
typedef struct wldcore_menu_sound_novel_history_level {
    s32 upper_window;   /* 0x00 */
    s32 lower_window;   /* 0x04 */
    s32 render_index;   /* 0x08 */
    s32 newest_text_id; /* 0x0c; newest history text */
    s32 text_ids[3];    /* 0x10 */
    s32 scroll;         /* 0x1c */
    s32 phase;          /* 0x20; wldcore_sound_novel_history_phase_e */
    s32 step;           /* 0x24; 0..4 fade step */
} wldcore_menu_sound_novel_history_level_t;

/* Provisional: location-transition menu level. The handler at 0x8008e540
 * uses the leading window index, location id, pending result, and the
 * choice text ids collected when route travel ends. */
typedef struct wldcore_menu_location_transition_level {
    s32 window_index; /* 0x00 */
    u8 _unused_04[8];
    s32 location_id; /* 0x0c */
    u8 _unused_10[0x20];
    s32 pending_result;      /* 0x30 */
    s32 choice_text_ids[10]; /* 0x34; retained as location_list.entries after arrival */
} wldcore_menu_location_transition_level_t;

/* wldcore_menu_choice_panel_level_t.mode as wldcore_proposition_handle_accept_confirm_input steps it. */
typedef enum wldcore_accept_confirm_mode {
    WLDCORE_ACCEPT_CONFIRM_MODE_CHOOSE = 0,
    WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE = 1, /* nobody can go: a press shows the second message */
    WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE = 3,   /* a refusal message is up: a press closes the panel */
} wldcore_accept_confirm_mode_e;

/* The two-choice confirmation panel built by wldcore_window_build_yes_no_panel and stepped by
 * wldcore_proposition_handle_accept_confirm_input and wldcore_proposition_handle_dispatch_confirm_input. */
typedef struct wldcore_menu_choice_panel_level {
    s32 cursor_window; /* 0x00 */
    s32 render_index;  /* 0x04 */
    s32 row;           /* 0x08; 0 = yes, 1 = no */
    s32 mode;          /* 0x0c; wldcore_accept_confirm_mode_e */
    s32 header_window; /* 0x10 */
} wldcore_menu_choice_panel_level_t;

/* Leading 32-bit fields of a menu level that owns up to two window/render record
 * pairs; wldcore_menu_close_level_and_release_records releases each index
 * that is not -1. */
typedef struct wldcore_menu_slot_set {
    s32 result;   /* 0x00; the location menu step sets 1 on confirm, 0 on cancel */
    s32 window_a; /* 0x04 */
    s32 render_a; /* 0x08 */
    s32 window_b; /* 0x0c */
    s32 render_b; /* 0x10 */
} wldcore_menu_slot_set_t;

/* Provisional: the world-map cursor level stepped by
 * wldcore_menu_step_map_cursor_level. */
typedef struct wldcore_menu_map_cursor_level {
    s32 window_index;     /* 0x00; the map cursor window */
    s32 alt_window_index; /* 0x04; the wide-view window swapped in by 0x80 */
    s32 wide_view;        /* 0x08; non-zero while the wide view is shown */
    s32 snap_hit;         /* 0x0c; dot index returned by the snap probe */
    s32 pending_mode;     /* 0x10 */
} wldcore_menu_map_cursor_level_t;

/* Provisional: the location menu level as its step handler
 * wldcore_menu_step_location_menu_level sees it. */
typedef struct wldcore_menu_location_menu_level {
    wldcore_menu_slot_set_t slots; /* 0x00; slots.result is the confirmed choice */
    s32 delay;                     /* 0x14 */
    s32 fade;                      /* 0x18 */
    u8 _unused_1c[0x14];
    s32 entry_count; /* 0x30 */
} wldcore_menu_location_menu_level_t;

/* Provisional: the type-0x15 sound-novel level pushed by
 * wldcore_menu_push_sound_novel_level and stepped by
 * wldcore_menu_step_sound_novel_level: title, header and footer windows
 * around a bar render record. */
typedef struct wldcore_menu_sound_novel_level {
    s32 left_window;    /* 0x00; title window */
    s32 right_window;   /* 0x04; header window */
    s32 render_index;   /* 0x08; bar render record */
    s32 overlay_window; /* 0x0c; footer window */
    u8 _unused_10[0xc];
    s32 message;     /* 0x1c; argument of the push */
    s32 _unknown_20; /* 0x20; set to 1 at push */
    s32 countdown;   /* 0x24; 9999 at push */
    s32 phase;       /* 0x28; cleared at push */
    s32 fade_timer;  /* 0x2c; cleared at push */
} wldcore_menu_sound_novel_level_t;

/* Provisional: 0x5c-byte menu-stack records at 0x800bb930, one per menu
 * level; the level's handler type in g_wldcore_menu_stack_types selects the
 * view. The leading 32-bit value normally names a window; the focus-location level
 * stores a map-dot index instead. g_wldcore_menu_stack_records_next is
 * the record array offset by one level, which the push routines index by the
 * current depth; spelling that as g_wldcore_menu_stack_records[depth + 1]
 * changes their address arithmetic. g_wldcore_menu_stack_types_next is the
 * same view of the type array, kept only where types[depth + 1] reschedules
 * the push routine (proposition detail, tutorial topic text, brave-story and
 * 0x03c0-0x03ff variable lists). */
typedef union wldcore_menu_stack_record {
    s32 window_index;
    s32 focus_dot_index;   /* WLDCORE_MENU_LEVEL_FOCUS_LOCATION */
    s32 story_event_index; /* WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT */
    wldcore_window_layout_request_t layout_window;
    wldcore_menu_message_level_t message;
    wldcore_menu_return_level_t return_level;
    wldcore_menu_panel_level_t panel;
    wldcore_window_render_pair_t window_render;
    wldcore_menu_window_pair_level_t window_pair;
    wldcore_menu_window_pair_render_level_t window_pair_render;
    wldcore_menu_countdown_t countdown; /* pushed by 0x8007a508 */
    wldcore_menu_rumor_detail_level_t rumor_detail;
    wldcore_menu_list_window_level_t list_window;
    wldcore_menu_proposition_list_level_t proposition_list;
    wldcore_menu_variable_list_level_t variable_list;
    wldcore_menu_participant_level_t participant;
    wldcore_menu_participant_list_level_t participant_list;
    wldcore_menu_proposition_cancel_confirm_level_t proposition_cancel_confirm;
    wldcore_menu_proposition_detail_panel_t proposition_detail_panel;
    wldcore_menu_variable_detail_level_t variable_detail;
    wldcore_menu_event_transition_level_t event_transition;
    wldcore_menu_sound_novel_resume_level_t sound_novel_resume;
    wldcore_menu_screen_transition_level_t screen_transition;
    wldcore_menu_scrollable_text_level_t scrollable_text;
    wldcore_menu_save_confirm_level_t save_confirm;
    wldcore_menu_sound_novel_history_level_t sound_novel_history;
    wldcore_menu_location_transition_level_t location_transition;
    wldcore_menu_send_unit_level_t send_unit;
    wldcore_menu_choice_panel_level_t choice_panel;
    wldcore_proposition_report_confirm_level_t proposition_report_confirm;
    wldcore_text_list_panel_t text_list;
    wldcore_menu_map_cursor_level_t map_cursor;
    wldcore_map_visibility_level_t visibility;
    wldcore_map_path_level_t path;
    wldcore_menu_location_menu_level_t location_menu;
    wldcore_location_list_level_t location_list;
    wldcore_proposition_report_level_t proposition_report;
    wldcore_menu_sound_novel_level_t sound_novel;
    wldcore_proposition_result_level_t proposition_result;
} wldcore_menu_stack_record_t;
typedef char wldcore_menu_stack_record_size_must_be_92[sizeof(wldcore_menu_stack_record_t) == 92 ? 1 : -1];

/* Verified fields of the 52-byte render records at 0x800bbc70. The pool init
 * 0x8006ad28 sets flags = 2, so the display-object dispatcher draws them as
 * sprites through wldcore_gfx_draw_display_object_sprite. */
typedef struct wldcore_window_render_record {
    s32 flags; /* 0x00; world_render_record_flag_e */
    s32 tpage;
    s32 priority; /* 0x08; OT priority passed to the sprite sort; 1 from the pool init */
    s32 coordinate_mode;
    s32 anim_counter; /* 0x10; flag-0x100 open-animation counter, cleared on every rebind */
    /* 0x14; CLUT selector: nonzero gives cx = clut_x + (palette - 1) << 4. The window-state
     * routines at 0x80076744/0x8007688c clear it or set it to 2. */
    s32 palette;
    point32_t base; /* 0x18 */
    point32_t clut; /* 0x20; (0x3c0, 0x1f0) from the pool init */
    /* 0x28; image position and size, copied and passed by value as one struct; the width is read signed */
    RECT bounds;
    CVECTOR color; /* 0x30 */
} wldcore_window_render_record_t;
typedef char wldcore_window_render_record_size_must_be_52[sizeof(wldcore_window_render_record_t) == 52 ? 1 : -1];

extern s16 g_wldcore_panel_row_actions[16];

extern s16 g_wldcore_active_menu_value;
void wldcore_draw_close_indicator_if_pending(GsOT* ot);

/* bits */

void wldcore_set_bit_value(u32* bits, s32 index, s32 set);
s32 wldcore_test_bit(u32* bits, s32 index);

void wldcore_write_nine_bit_record(u32* bits, s32 record, calendar_date_t* source);
void wldcore_read_nine_bit_record(u32* bits, s32 record, calendar_date_t* dest);

#endif

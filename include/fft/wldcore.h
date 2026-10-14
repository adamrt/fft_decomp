#ifndef WLDCORE_H
#define WLDCORE_H

/* WORLD/WLDCORE.BIN: the world core that runs with WORLD. */

#include "fft/world.h"

/* calendar */
extern u8 g_wldcore_calendar_days_per_month[12]; /* 0x8009e66c; non-leap-year month lengths */

extern s32 g_wldcore_calendar_display_x;
extern s32 g_wldcore_calendar_display_y;

/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern calendar_date_t g_wldcore_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];
void wldcore_calendar_advance_day(void);
void wldcore_calendar_convert_date_to_zodiac(s32* month, s32* day);
void wldcore_calendar_advance_days(s32 days);
s32 wldcore_calendar_calculate_day_of_year(s32 month, s32 day);
void wldcore_calendar_calculate_month_and_day(s32 day_of_year, s32* output_month, s32* output_day);

/* file */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream);
void wldcore_file_seek_to_battle_bin(void);

void wldcore_file_wait_then_build_header(
    main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);

void wldcore_wait_for_file_load(void);

void wldcore_load_scratch_data_blocking(void);
/* input */
extern u32 g_wldcore_input_current_buttons;
extern u32 g_wldcore_input_new_button_presses;
extern u32 g_wldcore_input_polled_buttons;
extern u32 g_wldcore_input_previous_buttons;
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
/* Interpreter state of the sound-novel script (wldcore_saved_record_t.state_flags),
 * stepped by wldcore_script_run_until_yield. */
typedef enum wldcore_opcode_state_flag {
    WLDCORE_OPCODE_STATE_BRANCH = 0x002,  /* a branch was taken: mark its saved-data bit and enter the next section */
    WLDCORE_OPCODE_STATE_FETCH = 0x004,   /* the handler is done: load the next 32-bit instruction */
    WLDCORE_OPCODE_STATE_YIELD = 0x008,   /* leave the interpreter until the next frame */
    WLDCORE_OPCODE_STATE_FETCHED = 0x010, /* the 32-bit instruction was loaded this turn: the handler's first call */
    WLDCORE_OPCODE_STATE_END = 0x020,     /* the script ended */
    WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE = 0x040,
    WLDCORE_OPCODE_STATE_VALUE_DISPLAY = 0x080, /* the numeric value display is up */
    WLDCORE_OPCODE_STATE_WHITE_OVERLAY = 0x100, /* the white full-screen overlay is up */
} wldcore_opcode_state_flag_e;

/* The world-script state from the 32-bit instruction at 0x800d4848 onwards
 * (the tail of wldcore_saved_record_t). Handlers must reach it as
 * in-struct references: the target keeps each state_flags store ahead of the
 * window/render-record updates and reloads render_index after them, which GCC
 * only does for in-struct accesses. The instruction is an encoded 32-bit value;
 * g_wldcore_state_flags remains a separate scalar view for handlers whose
 * flag loads need their own symbol base. */
typedef struct wldcore_opcode_state {
    u32 instruction; /* 0x00 */
    u16 state_flags; /* 0x04; g_wldcore_state_flags; wldcore_opcode_state_flag_e */
    u8 _unused_06[0x0e];
    s16 render_index; /* 0x14 */
    s16 picture;      /* 0x16 */
    u8 _unused_18[0x0e];
    s16 x; /* 0x26 */
    s16 y; /* 0x28 */
} wldcore_opcode_state_t;

/* Scalar view of g_wldcore_active_saved_record.state_flags for opcode
 * handlers that read-modify-write it beside another record store: the member
 * spelling shares one base register where the target uses absolute addresses. */
extern u16 g_wldcore_state_flags;

extern wldcore_opcode_state_t g_wldcore_opcode_state;
extern void (*g_wldcore_opcode_handlers[])(void);
void wldcore_opcode_apply_pending_delta_and_clamp_value(void);
void wldcore_opcode_branch_if_saved_bit(s32 expected);
void wldcore_opcode_branch_if_local_flag(s32 expected);
void wldcore_opcode_branch_if_savedata_bit(s32 expected);
void wldcore_opcode_branch_if_dialog_option(s32 expected);
void wldcore_opcode_map_request_deep_dungeon_entry(void);
void wldcore_opcode_map_request_draw_path(void);
void wldcore_opcode_map_request_erase_path(void);
void wldcore_opcode_map_request_load_event(void);
void wldcore_opcode_map_request_set_var_2(void);
void wldcore_opcode_map_request_text(void);
void wldcore_opcode_start_pending_value_transition(void);
void wldcore_opcode_load_picture(void);
void wldcore_opcode_load_background_set(void);

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

extern s32 g_wldcore_random_battle_entd_id;
extern s32 g_wldcore_random_battle_map_id;
extern s32 g_wldcore_random_battle_squad_id;
extern wldcore_encounter_entry_t (*g_wldcore_random_battle_data)[3];
extern wldcore_deep_dungeon_battle_t* g_wldcore_deep_dungeon_battles;

/* route */
enum { WLDCORE_ROUTE_PATH_CAPACITY = 16 };

/* A path entry packs the route id in its low byte and the direction in bit 8. */
enum {
    WLDCORE_ROUTE_PATH_ID_MASK = 0xff,
    WLDCORE_ROUTE_PATH_REVERSED = 0x100,
};

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

/* A route header followed by count travel points. */
typedef struct wldcore_route_record {
    u8 count;
    u8 start_location;
    u8 end_location;
    u8 cost;
    wldcore_route_point_t points[0]; /* 0x04; variable length */
} wldcore_route_record_t;
typedef char wldcore_route_record_size_must_be_4[sizeof(wldcore_route_record_t) == 4 ? 1 : -1];

extern s32 g_wldcore_route_search_depth;
extern s32 g_wldcore_route_search_cost;
extern wldcore_route_path_t g_wldcore_route_search_path;

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

/* Rows of four sound numbers in the WLDCORE sound bank (id 0xE1), indexed by
 * a sound id from 1. Row 0 is not a sound row: 0x8009ec18 holds the eighth
 * 32-bit value of g_wldcore_tutorial_event_ids (0x1a8). */
extern u8 g_wldcore_sound_quad_command_bytes[][4];
extern s32 g_wldcore_sound_novel_script_sector_offsets[];
extern s32 g_wldcore_sound_novel_text_sector_offsets[];

/* Sound resource the sound-novel menu passes to suzuki_sound_append_resource. */
extern struct suzuki_sound_resource* g_wldcore_default_sound_novel_sound_resource;
extern struct suzuki_sound_resource* g_wldcore_sound_novel_sound_resource;
extern s32 g_wldcore_sound_novel_countdown_frames;
extern s32 g_wldcore_sound_novel_countdown_section;
extern s32 g_wldcore_sound_novel_countdown_elapsed;
extern u32 g_wldcore_sound_novel_local_flags[];
extern s32 g_wldcore_sound_novel_wait_counter;
extern wldcore_audio_queue_t g_wldcore_audio_queue;

/* Unsigned views of g_wldcore_active_saved_record.text_history_0..3 for the
 * two history shifters, which load them with lhu; the record keeps s16 for
 * the -1 stores of wldcore_reset_saved_record_fields. */
extern s16 g_wldcore_sound_novel_text_history_0;
extern u16 g_wldcore_sound_novel_text_history_1;
extern u16 g_wldcore_sound_novel_text_history_2;
extern u16 g_wldcore_sound_novel_text_history_3;
extern s32 g_wldcore_sound_novel_wait_frames;
extern u8 g_wldcore_sound_novel_quit_help_text_ids[];
void wldcore_sound_process_audio_queue(void);
void wldcore_sound_change_scenario_music(s32 music_id);
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value);
void wldcore_sound_enqueue_music_start(s32 value);
void wldcore_sound_play_effect(s32 sound_id);
void wldcore_sound_stop_music_and_play_sound(s32 sound_id);
void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id);
void wldcore_sound_wait_for_queue_drain(void);
void wldcore_sound_novel_restore_saved_state(s32 render_index);
void wldcore_sound_play_pending_script_sounds(s32 low, s32 high, s32 value);
s32 wldcore_sound_novel_update_countdown_timer(void);
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
extern menu_cursor_state_t g_wldcore_story_events_cursor_state;
extern u8 (*g_wldcore_brave_story_birthdays)[2];
extern u8 g_wldcore_brave_story_help_text_ids[];
extern s32 g_wldcore_brave_story_saved_cursor[];
extern u16* g_wldcore_story_event_ids;

/* Provisional: milestone threshold tables at 0x8009eb34 (eight entries,
 * 1/4/8/12/16/20/24/31) and 0x8009eb3c (1/3/6/8/10/12/14/16). */
extern u8 g_wldcore_treasure_count_milestones[];
extern u8 g_wldcore_treasure_picture_sets[];
extern u8 g_wldcore_treasure_sound_novel_ids[];
extern u8 g_wldcore_unexplored_land_count_milestones[];
extern u8 g_wldcore_unexplored_land_picture_sets[];
void wldcore_advance_brave_story_birthdays(void);

/* system */
/* g_main_system_flags as wldcore_process_frame_gate last saw it (main_system_flags_e). */
extern u32 g_wldcore_previous_system_flags;

void wldcore_restore_previous_stack(void);
void wldcore_init_stub(void);
void wldcore_reset_game_if_special_keycode_is_pressed(void);

void wldcore_init_world_map_state(void);
void wldcore_switch_to_stack(void*);
s32 wldcore_handle_exit_request(void);
s32 wldcore_entrypoint(void);
void wldcore_finalize_loop(void);
void wldcore_init_core(void);
void wldcore_init_runtime_state(void);
void wldcore_init_subsystems(void);
void wldcore_process_frame_gate(void);
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
s32 wldcore_get_ramza_roster_index(void);
s32 wldcore_unit_get_battle_id_by_runtime_id(s32 runtime_id);
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
 * selects the handler in wldcore_dispatch_display_object_list:
 * bit 0 an animated object (world_animated_render_record_t), bit 1 a sprite
 * (wldcore_window_render_record_t). */
struct wldcore_window_render_record;

extern GsOT_TAG g_wldcore_gfx_aux_ot_tags[2][4];
extern GsOT g_wldcore_gfx_aux_ordering_tables[2];
extern GsOT_TAG g_wldcore_gfx_ot_tags[2][16];
extern GsOT g_wldcore_gfx_ordering_tables[2];
extern PACKET g_wldcore_gfx_world_primitive_buffers[];
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
extern s32 g_wldcore_loaded_background_set;

/* Currently resident picture set, reset to -1 by wldcore_init_world_map_state. */
extern s32 g_wldcore_loaded_picture_set;
extern u16* g_wldcore_dissolve_band_order;
extern u8 g_wldcore_dissolve_band_progress[16];

/* Destination image buffer (read back as a TIM by wldcore_opcode_load_picture). */
extern u32* g_wldcore_picture_buffer;
extern u32* g_wldcore_scratch_buffer;

/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern rgb24_t g_wldcore_screen_brightness_rgb;
extern wldcore_sortbox_state_t g_wldcore_screen_fade_state;

void wldcore_gfx_draw_world_frame_with_map(void);
s32 wldcore_fade_step_screen(void);
void wldcore_gfx_toggle_captured_world_frame(s32 capture);
void wldcore_gfx_draw_projected_map_tiles(GsOT* ot);
void wldcore_gfx_draw_animated_display_object(world_animated_render_record_t* object, GsOT* ot);
void wldcore_gfx_draw_context_value_display(GsOT* ot);
void wldcore_gfx_draw_display_object_sprite(struct wldcore_window_render_record* object, GsOT* ot);
void wldcore_gfx_draw_calendar_date(GsOT* ot);
void wldcore_clear_screen_and_set_map_clip_rect(s32 mode);
void wldcore_dispatch_display_object_list(GsOT* ot, void** list, s32 count);
void wldcore_fade_draw_screen_overlays(void);
void wldcore_fade_start_screen(s32 flags, s32 duration);
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch);
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out);
void wldcore_gfx_draw_world_frame(void);
void wldcore_gfx_init_subsystem(void);
void wldcore_gfx_set_display_rect(s32 buffer_index, RECT* rect);
void wldcore_start_core_background_threads(s32 party_index);
void wldcore_gfx_draw_number_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot);
void wldcore_gfx_draw_number_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad);
void wldcore_gfx_draw_cursor_sprite_and_underline(GsOT* ot, GsSPRITE* sprite, GsLINE* line);

s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* im, s32 step);

void wldcore_gfx_load_background_set(s32 set);
void wldcore_gfx_prepare_play_time_display(void);
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

/* wldcore_location_entry_state_t.flags. */
typedef enum wldcore_route_travel_flag {
    WLDCORE_ROUTE_TRAVEL_ACTIVE = 0x01,   /* a path was found and is being traveled */
    WLDCORE_ROUTE_TRAVEL_REVERSED = 0x02, /* the current leg runs its route end to start */
} wldcore_route_travel_flag_e;

/* Provisional: location-entry state at 0x800d0b24, filled by the route search
 * wldcore_map_find_best_route_path, staged per route leg by wldcore_location_begin_route_segment and stepped by
 * wldcore_map_step_route_travel. The vertices are SVECTORs: their 2-byte alignment is what
 * makes `previous_position = position` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) that the target emits. */
typedef struct wldcore_location_entry_state {
    s32 flags;                       /* 0x00; wldcore_route_travel_flag_e */
    s32 route_id;                    /* 0x04; route & 0xff, index into g_wldcore_map_route_tables */
    s32 segment_index;               /* 0x08; current segment of the leg, the step for begin_route_segment */
    s32 segment_count;               /* 0x0c; wldcore_location_begin_route_segment result (record count - 1) */
    s32 segment_frame;               /* 0x10; frames into the segment, interpolation numerator */
    s32 segment_frames;              /* 0x14; point distance * 2, interpolation denominator */
    wldcore_route_path_t route_path; /* 0x18; best path found */
    s32 route_index;                 /* 0x58; route_path.routes index of the current leg */
    s32 best_path_length;            /* 0x5c */
    s32 best_cost;                   /* 0x60; reset to 99999 */
    s32 best_length_bound;           /* 0x64; reset to 99999 */
    s32 leg_start_location;          /* 0x68; route record start (end when reversed) */
    s32 leg_end_location;            /* 0x6c; becomes the current location and script variable 0x31 on arrival */
    s32 heading;                     /* 0x70; route point heading (+0x800 when reversed), copied to the marker angle */
    s32 menu_level;                  /* 0x74; parent menu depth */
    SVECTOR previous_position;       /* 0x78 */
    SVECTOR position;                /* 0x80 */
    SVECTOR target_position;         /* 0x88 */
    s32 arrival_location;            /* 0x90; arrival location id + 1 */
} wldcore_location_entry_state_t;
typedef char wldcore_location_entry_state_size_must_be_148[sizeof(wldcore_location_entry_state_t) == 148 ? 1 : -1];

extern u8 g_wldcore_choice_hidden_variable_offsets[];
extern wldcore_location_entry_state_t g_wldcore_location_entry_state;
extern wldcore_state_record_t* g_wldcore_location_records;
extern s32 g_wldcore_location_marker_shade;
extern u32* g_wldcore_location_frame_tim;
s32 wldcore_location_begin_route_segment(wldcore_location_entry_state_t* state, u32 value, s32 step);
s32 wldcore_location_collect_choice_text_ids(s32 location_id, s32* out);
s32 wldcore_location_process_entry(s32 from_location, s32 to_location);

/* save */
/* Section byte offsets at the start of a loaded sound-novel script. */
typedef struct wldcore_sound_novel_script_file {
    u32 _unknown_00;
    u32 offsets[0]; /* 0x04; variable-length section byte-offset table */
} wldcore_sound_novel_script_file_t;

extern s16 g_wldcore_saved_record_index;

/* Saved copy of g_wldcore_context_value_display_position. */
extern point32_t g_wldcore_saved_context_value_display_position;
extern wldcore_saved_record_t g_wldcore_active_saved_record;
/* g_main_system_flags as saved by wldcore_gfx_toggle_captured_world_frame (main_system_flags_e). */
extern u32 g_wldcore_saved_system_flags;
void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record);
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

typedef enum wldcore_script_request_flags {
    WLDCORE_SCRIPT_REQUEST_CHOICE = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_CHOICE,
    WLDCORE_SCRIPT_REQUEST_LOAD_EVENT = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_LOAD_EVENT,
    WLDCORE_SCRIPT_REQUEST_TEXT = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_TEXT,
    WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY
    = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY,
    WLDCORE_SCRIPT_REQUEST_SET_VAR_2 = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_SET_VAR_2,
    WLDCORE_SCRIPT_REQUEST_DRAW_PATH = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DRAW_PATH,
    WLDCORE_SCRIPT_REQUEST_ERASE_PATH = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_ERASE_PATH,
    WLDCORE_SCRIPT_REQUEST_FOCUS_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION,
    WLDCORE_SCRIPT_REQUEST_DRAW_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DRAW_LOCATION,
    WLDCORE_SCRIPT_REQUEST_ERASE_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_ERASE_LOCATION,
} wldcore_script_request_flags_e;

/* WLDCORE script interpreter state; the opcode handlers read ip/data here.
 * One object through the choice/argument 32-bit values:
 * wldcore_opcode_read_four_halfword_pairs reaches ip at -4 and the choice
 * values at +0x10 from its &choice_vars base. */
typedef struct wldcore_script_state {
    s32 flags;          /* 0x00; wldcore_script_result_flags_e | wldcore_script_action_flags_e */
    u16* list;          /* 0x04; script list for the current location */
    u16* data;          /* 0x08 */
    u16 ip;             /* 0x0c */
    s32 choice_vars[4]; /* 0x10; staged Choice variables */
    /* 0x20: script request arguments; the staged Choice values share them. */
    s32 args[4];
} wldcore_script_state_t;
typedef char wldcore_script_state_size_must_be_48[sizeof(wldcore_script_state_t) == 48 ? 1 : -1];

extern u16* g_wldcore_default_script_base;
extern u16* g_wldcore_script_base;
extern void (*g_wldcore_script_opcode_table[])(void);
extern wldcore_script_state_t g_wldcore_script_state;

/* Scalar view of g_wldcore_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses.
 * The load_* opcode handlers declare volatile views of flags/data/ip locally. */
extern s32 g_wldcore_script_flags;
s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask);
void wldcore_script_read_operand_pair(s32* out_id, s32* out_value);
void wldcore_script_read_operand_pair_and_date(s32* out_a, s32* out_b, s32* out_c, s32* out_d);
s32 wldcore_script_run_until_yield(void);
u32 wldcore_sound_novel_find_section_instruction(s32 section, s32 opcode);

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

/* wldcore_text_scrollable_window_t.scroll_state. */
typedef enum wldcore_text_scroll_state {
    WLDCORE_TEXT_SCROLL_PAGING = 0,
    WLDCORE_TEXT_SCROLL_MANUAL = 1, /* while square is held: up and down scroll by rows */
    WLDCORE_TEXT_SCROLL_RETURN = 2, /* scrolls back to the page start */
} wldcore_text_scroll_state_e;

/* The text ids of TEXT_ID_WORLD_MENU_BASE that the code names: the bar, the
 * Brave Story, the tutorial and the location menu. The first five entries are
 * the bar's rows, TEXT_ID_WORLD_MENU_BASE + wldcore_bar_menu_row_e. */
typedef enum wldcore_text_id {
    WLDCORE_TEXT_ID_OK_NO = 0xb805,                 /* "O K / N O" */
    WLDCORE_TEXT_ID_BAR_WELCOME = 0xb806,           /* the bar owner: "Welcome. Come in and relax." */
    WLDCORE_TEXT_ID_BAR_GOODBYE = 0xb807,           /* "Thank you! Come again." */
    WLDCORE_TEXT_ID_RUMORS_INTRO = 0xb808,          /* "Rumors are floating around." */
    WLDCORE_TEXT_ID_RUMOR_ABOUT = 0xb809,           /* "This rumor's about ..." */
    WLDCORE_TEXT_ID_NO_RUMORS = 0xb80a,             /* "Haven't heard any rumors yet." */
    WLDCORE_TEXT_ID_PROPOSITIONS_INTRO = 0xb80b,    /* "I know a great way to make a buck." */
    WLDCORE_TEXT_ID_PROPOSITION_ABOUT = 0xb80c,     /* "This's what I know about ..." */
    WLDCORE_TEXT_ID_NO_PROPOSITIONS = 0xb80d,       /* "Haven't heard any offers lately." */
    WLDCORE_TEXT_ID_NO_MORE_JOBS = 0xb80e,          /* "I can't give anymore jobs." */
    WLDCORE_TEXT_ID_ACCEPT_JOB_PROMPT = 0xb80f,     /* "You want this job?" */
    WLDCORE_TEXT_ID_JOB_FEE_PROMPT = 0xb810,        /* "You need ... gil for info etc. Still wanna do it?" */
    WLDCORE_TEXT_ID_NOT_ENOUGH_GIL = 0xb811,        /* "You don't got enough gil." */
    WLDCORE_TEXT_ID_CHOOSE_UNITS = 0xb812,          /* "So, who you wanna send?" */
    WLDCORE_TEXT_ID_CHOOSE_DAYS = 0xb813,           /* "This job's from ... to .... How many days you want?" */
    WLDCORE_TEXT_ID_CONFIRM_CONDITIONS = 0xb814,    /* "These are the conditions, OK?" */
    WLDCORE_TEXT_ID_JOBS_RETURNED = 0xb819,         /* "Good job. You came back from these jobs." */
    WLDCORE_TEXT_ID_HEAR_ABOUT_JOB = 0xb81a,        /* "Wanna hear about ...?" */
    WLDCORE_TEXT_ID_JOBS_IN_PROGRESS = 0xb81b,      /* "These jobs're in progress." */
    WLDCORE_TEXT_ID_RETURN_TOMORROW = 0xb81c,       /* "I'll have them return tomorrow." */
    WLDCORE_TEXT_ID_CALL_BACK_PROMPT = 0xb81d,      /* "What a waste if they quit now. Still wanna call them back?" */
    WLDCORE_TEXT_ID_CALL_BACK_ACCEPTED = 0xb81e,    /* "Ok, I'll have them return by tomorrow." */
    WLDCORE_TEXT_ID_JOB_CONDITION_LABELS = 0xb821,  /* "Days", "Place", "Unit" with the first two filled in */
    WLDCORE_TEXT_ID_CHOOSE_MORE_UNITS = 0xb822,     /* "Who else are you sending?" */
    WLDCORE_TEXT_ID_DAYS_LABEL = 0xb823,            /* "Days:" */
    WLDCORE_TEXT_ID_PARTY_NUMBER_LABEL = 0xb826,    /* "Party number:" */
    WLDCORE_TEXT_ID_JOB_RESULT_LABELS = 0xb830,     /* "Bonus", "Total", "Job Point" */
    WLDCORE_TEXT_ID_FOUR_DIGIT_VALUE = 0xb831,      /* a value, four digits wide */
    WLDCORE_TEXT_ID_QUIT_CONTINUE = 0xb83a,         /* "Quit / Continue" */
    WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST = 0xb83f, /* plus wldcore_brave_story_row_e: "Fact" to "Return" */
    WLDCORE_TEXT_ID_INJURED_CASUALTIES = 0xb846,    /* "Injured", "Casualties" and their counts */
    WLDCORE_TEXT_ID_BRAVE_STORY_INTRO = 0xb848, /* Alazlam: "The secrets of the Lion War are uncovered in this book." */
    WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO = 0xb849, /* "This is the information on ..." */
    WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE = 0xb84a,    /* "You must find truth on your own. ..." */
    WLDCORE_TEXT_ID_SOUND_NOVEL_RESUME_CHOICES
    = 0xb84b,                                    /* "This book was canceled earlier. Restart / Read from start" */
    WLDCORE_TEXT_ID_SUBSTITUTED_TEXT = 0xb84c,   /* the text of substitution value 0 alone */
    WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL = 0xb84e, /* "Job contents:" */
    WLDCORE_TEXT_ID_MONTH_NAME_BASE = 0xb84f,    /* plus the month, 1 to 12: "Jan" to "Dec" */
    WLDCORE_TEXT_ID_TOLD_BEFORE = 0xb85c,        /* "Think I told you before but..." */
    WLDCORE_TEXT_ID_BAR = 0xb85d,                /* the four entries of a town's location menu */
    WLDCORE_TEXT_ID_SHOP = 0xb85e,
    WLDCORE_TEXT_ID_SOLDIER_OFFICE = 0xb85f,
    WLDCORE_TEXT_ID_FUR_SHOP = 0xb860,
    WLDCORE_TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST
    = 0xb864,                                 /* plus wldcore_proposition_category_row_e: "Advancing", "Work history" */
    WLDCORE_TEXT_ID_VALUE = 0xb866,           /* a value */
    WLDCORE_TEXT_ID_TWO_DIGIT_VALUE = 0xb867, /* a value, two digits wide */
    WLDCORE_TEXT_ID_RAMZA_INFO = 0xb868,      /* BRAVE_STORY_ENTRY_INFO for person 0: a title and Ramza's name */
    WLDCORE_TEXT_ID_RAMZA_TITLE_FIRST = 0xb869,      /* plus his profile: sixteen titles, "Knight" each */
    WLDCORE_TEXT_ID_PROPOSITION_HINT_FIRST = 0xb879, /* plus the proposition: the bar owner's advice on it */
    WLDCORE_TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST
    = 0xb8d9,                                   /* plus wldcore_tutorial_category_row_e: "Organization" to "Return" */
    WLDCORE_TEXT_ID_TUTORIAL_WELCOME = 0xb8dd,  /* Daravon: "If you know, you can foresee. ..." */
    WLDCORE_TEXT_ID_TUTORIAL_PROMPT = 0xb8de,   /* "Know the path to win the battle. Solve your questions here." */
    WLDCORE_TEXT_ID_CHOOSE_HUMAN_UNIT = 0xb8e0, /* "Choose a good human to send, or one with no strings!" */
    WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST = 0xb8ed, /* plus the floor: "Nogias" to "End" */
    WLDCORE_TEXT_ID_TREASURE_FOUND_FIRST
    = 0xb8f9, /* the eleven messages for a treasure found, to "Treasure Hunter Master" */
    WLDCORE_TEXT_ID_LAND_FOUND_FIRST = 0xb904, /* the same for an unexplored land, to "Master Adventurer" */
} wldcore_text_id_e;

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
extern s32 g_wldcore_displayed_numeric_value;
extern s32 g_wldcore_displayed_numeric_value_secondary;
extern s32 g_wldcore_displayed_numeric_value_tertiary;
extern s32 g_wldcore_loaded_message_block;
void wldcore_text_render_scrollable_window_page(wldcore_text_scrollable_window_t* state, s32 row);
void wldcore_load_message_block_if_changed(s32 block);
void wldcore_text_get_padded_dimensions(s32 text_id, size32_t* output);
void wldcore_text_init_scrollable_window(wldcore_text_scrollable_window_t* state);
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* window);
void wldcore_text_update_scroll_indicators(wldcore_text_scrollable_window_t* state);

void wldcore_text_update_page_prompt_visibility(wldcore_text_scrollable_window_t* state);
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

/* Outcome of a proposition (wldcore_proposition_report_t.result), from the
 * participants' total score. Only SUCCESS completes the proposition and earns
 * its reward. */
typedef enum wldcore_proposition_result {
    WLDCORE_PROPOSITION_RESULT_SUCCESS = 0,     /* score of 99 or more, or the preferred-job roll */
    WLDCORE_PROPOSITION_RESULT_FAILURE = 1,     /* score of 59 to 98: a tenth of the JP, a fifth of the gil */
    WLDCORE_PROPOSITION_RESULT_BAD_FAILURE = 2, /* lower score, or recalled: a twentieth of both */
} wldcore_proposition_result_e;

/* What a successful proposition brought back (wldcore_proposition_report_t.reward_type);
 * TREASURE and UNEXPLORED_LAND are also the proposition row's discovery_kind. */
typedef enum wldcore_proposition_reward {
    WLDCORE_PROPOSITION_REWARD_NONE = 0,
    WLDCORE_PROPOSITION_REWARD_TREASURE = 1,
    WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND = 2,
    WLDCORE_PROPOSITION_REWARD_RANDOM_BONUS = 3, /* the three-tier random reward */
} wldcore_proposition_reward_e;

/* Provisional: proposition-resolution work record at 0x8009f1ec. One object:
 * wldcore_proposition_find_preferred_job_unit reads excluded_party_index at
 * -0xc from its &proposition_index base, and calculate_base_jp_and_gil and
 * step_result_level reach -0x10..+0x24 from single bases. */
typedef struct wldcore_proposition_report {
    s32 excluded_party_index; /* 0x00 */
    s32 speaker_party_index;  /* 0x04; chosen and read back by wldcore_proposition_step_participant_message */
    s32 preferred_unit;       /* 0x08; fixed speaker passed to wldcore_proposition_step_participant_message */
    s32 proposition_index;    /* 0x0c; index into g_main_active_propositions */
    s32 result;               /* 0x10; wldcore_proposition_result_e */
    s32 reward_type;          /* 0x14; wldcore_proposition_reward_e */
    s32 reward_index;         /* 0x18 */
    s32 reward_value;         /* 0x1c */
    s32 recalled;             /* 0x20; reward calculation is skipped after a recall */
    u8 _unused_24[0x10];
    s32 participant_jp[3];     /* 0x34; JP awarded to each participant */
    s32 participant_scores[3]; /* 0x40; success-score contribution */
    s32 participant_gil[3];    /* 0x4c; contribution to the total gil reward */
} wldcore_proposition_report_t;
typedef char wldcore_proposition_report_size_must_be_88[sizeof(wldcore_proposition_report_t) == 88 ? 1 : -1];

/* wldcore_proposition_progress_t.flags of an active proposition. */
typedef enum wldcore_proposition_progress_flag {
    WLDCORE_PROPOSITION_PROGRESS_DISPATCHED = 0x01,
    WLDCORE_PROPOSITION_PROGRESS_RECALLED = 0x02, /* canceled: returns on the next day */
    WLDCORE_PROPOSITION_PROGRESS_RETURNED = 0x04, /* the days are up: waiting for the report */
} wldcore_proposition_progress_flag_e;

/* Value of a proposition's script variable (EVENT_SCRIPT_VAR_PROPOSITION_FIRST + id). */
typedef enum wldcore_proposition_variable_flag {
    WLDCORE_PROPOSITION_VARIABLE_IN_PROGRESS = 0x02,
    WLDCORE_PROPOSITION_VARIABLE_COMPLETED = 0x04,
    WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED = 0x08, /* dispatched at least once */
} wldcore_proposition_variable_flag_e;

/* g_main_proposition_states[]: days until a failed proposition is offered
 * again, and which failure it was. */
typedef enum wldcore_proposition_state {
    WLDCORE_PROPOSITION_STATE_DAYS_MASK = 0x3f,
    WLDCORE_PROPOSITION_STATE_FAILURE = 0x40,
    WLDCORE_PROPOSITION_STATE_BAD_FAILURE = 0x80,
} wldcore_proposition_state_e;

/* Records of the proposition data file (wldcore_proposition_get_data_pointer). */
typedef enum wldcore_proposition_data_record {
    WLDCORE_PROPOSITION_DATA_ROWS = 0,                /* 23-byte proposition rows */
    WLDCORE_PROPOSITION_DATA_FEES = 1,                /* 16-bit values behind the rows' two fee indices */
    WLDCORE_PROPOSITION_DATA_MESSAGE_ADJUSTMENTS = 2, /* by faith and bravery band */
    WLDCORE_PROPOSITION_DATA_CATEGORY_JOB_SCORES = 3,
    WLDCORE_PROPOSITION_DATA_JOB_LIST_JOB_SCORES = 4,
    WLDCORE_PROPOSITION_DATA_BRAVERY_LEVEL_SCORES = 5,
    WLDCORE_PROPOSITION_DATA_FAITH_SCORES = 6,
    WLDCORE_PROPOSITION_DATA_JP_AMOUNTS = 8,
    WLDCORE_PROPOSITION_DATA_GIL_AMOUNTS = 9,
    WLDCORE_PROPOSITION_DATA_DISCOVERY_REWARDS = 10,
    WLDCORE_PROPOSITION_DATA_RANDOM_REWARDS = 11,
    WLDCORE_PROPOSITION_DATA_EXTRA_DAY_JP_SCALES = 12,
    WLDCORE_PROPOSITION_DATA_EXTRA_DAY_GIL_SCALES = 13,
    WLDCORE_PROPOSITION_DATA_JOB_LISTS = 14, /* nine job ids per job list */
} wldcore_proposition_data_record_e;

/* Records 10 and 11 select one entry from each reward-amount table. */
typedef struct wldcore_proposition_reward_pair {
    u8 gil_index;
    u8 jp_index;
} wldcore_proposition_reward_pair_t;

/* wldcore_proposition_report_level_t.load_phase. */
typedef enum wldcore_proposition_report_load {
    WLDCORE_PROPOSITION_REPORT_LOAD_DONE = 0,
    WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES = 1, /* waits for the audio queue, then loads the message block */
    WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE = 2,  /* waits for it, then loads the reward's picture set */
} wldcore_proposition_report_load_e;

/* wldcore_proposition_result_level_t.phase. */
typedef enum wldcore_proposition_result_phase {
    WLDCORE_PROPOSITION_RESULT_PHASE_INPUT = 0,
    WLDCORE_PROPOSITION_RESULT_PHASE_CLOSING = 1, /* waits for the audio queue and the message thread */
    WLDCORE_PROPOSITION_RESULT_PHASE_DONE = 2,    /* waits for the audio queue, then pops */
} wldcore_proposition_result_phase_e;

/* Which participant reports (proposition row speaker_mode). */
typedef enum wldcore_proposition_speaker {
    WLDCORE_PROPOSITION_SPEAKER_HIGHEST_BRAVERY = 1,
    WLDCORE_PROPOSITION_SPEAKER_LOWEST_BRAVERY = 2,
    WLDCORE_PROPOSITION_SPEAKER_HIGHEST_FAITH = 3,
    WLDCORE_PROPOSITION_SPEAKER_LOWEST_FAITH = 4,
} wldcore_proposition_speaker_e;

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

typedef enum wldcore_proposition_condition_flag {
    WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F = 0x01,
    WLDCORE_PROPOSITION_CONDITION_MONTH = 0x02,
    WLDCORE_PROPOSITION_CONDITION_STATUS = 0x04,
} wldcore_proposition_condition_flag_e;
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
extern wldcore_proposition_report_t g_wldcore_proposition_report;
extern s32* g_wldcore_proposition_data_base;
extern s32 g_wldcore_proposition_selected_entry;
extern s32 g_wldcore_proposition_send_unit_count;
extern wldcore_proposition_fields_t g_wldcore_selected_proposition_row[];
extern s32 g_wldcore_proposition_gil_amount;
extern s32 g_wldcore_proposition_send_units[];
extern s16 g_wldcore_proposition_candidate_levels[];
extern s32 g_wldcore_proposition_dispatch_days;

/* Saved brave-story row, declared as an array for the same reason as
 * g_wldcore_active_propositions_saved_cursor. */
extern s32 g_wldcore_proposition_category_saved_cursor[];
extern u32 g_wldcore_proposition_detail_image_buffer[];
void wldcore_proposition_prepare_report(s32 proposition_index);
void wldcore_proposition_handle_report_confirm_input(wldcore_proposition_report_confirm_level_t* level);
s32 wldcore_proposition_select_report_speaker(s32 proposition_index);
void wldcore_proposition_determine_success(void);
void wldcore_proposition_calculate_base_jp_and_gil(void);
void wldcore_proposition_roll_bonus_reward(void);
void wldcore_proposition_apply_extra_days_multiplier(void);
s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level);
s32 wldcore_proposition_adjust_message_index(s32 base, s32 party_index);
void wldcore_proposition_advance_daily_counters(void);
s32 wldcore_proposition_count_completed(s32 location);
/* File records have different layouts; the selected record determines the table type. */
void* wldcore_proposition_get_data_pointer(wldcore_proposition_data_record_e record);
s32 wldcore_proposition_load_picture(s32 index);
void wldcore_proposition_load_picture_data_set(s32 value);
s32 wldcore_proposition_step_message(wldcore_proposition_report_level_t* state, s32 message);
void wldcore_proposition_submit_marker_request(GsOT* ot);
void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index);
void wldcore_proposition_push_cancel_confirm_level(s32 value);
void wldcore_proposition_push_detail_level(s32 value);
s32 wldcore_proposition_is_available_at_location(s32 index, s32 location);
void wldcore_proposition_push_dispatch_confirm_level(void);
void wldcore_proposition_step_send_unit_slot_input(wldcore_menu_send_unit_level_t* level);
void wldcore_proposition_step_send_unit_browser(wldcore_menu_send_unit_level_t* level);
void wldcore_proposition_push_send_unit_level(void);
s32 wldcore_proposition_step_participant_message(wldcore_proposition_report_level_t* state, s32 message, s32 speaker);
void wldcore_proposition_push_result_level(s32 proposition);
s32 wldcore_map_job_id_to_category_index(s32 job_id);
s32 wldcore_proposition_find_preferred_job_unit(void);

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

/* Phases of a list-window level (wldcore_menu_list_window_level_t.mode). The
 * town menu uses only the first two. */
typedef enum wldcore_list_window_mode {
    WLDCORE_LIST_WINDOW_MODE_BROWSE = 0,
    WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE = 1, /* the farewell text is up; waits for circle or cross */
    WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP = 2,  /* fading out after the farewell, or under the help message */
    WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT = 3,    /* faded out; waits for thread 14 to end, then pops */
} wldcore_list_window_mode_e;

/* Row kinds of the town menu list (wldcore_bar_build_menu_rows); the row text
 * is 0xb800 + kind. */
typedef enum wldcore_bar_menu_row {
    WLDCORE_BAR_MENU_ROW_RUMORS = 0,
    WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS = 1,
    WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS = 2,
    WLDCORE_BAR_MENU_ROW_PROPOSITIONS = 3,
    WLDCORE_BAR_MENU_ROW_LEAVE = 4,
} wldcore_bar_menu_row_e;

/* Row kinds of the brave-story list (wldcore_menu_build_brave_story_entries),
 * named after the level each row opens. */
typedef enum wldcore_brave_story_row {
    WLDCORE_BRAVE_STORY_ROW_SCRIPT_FLAGS_01A4_01BB = 0,
    WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS = 1,
    WLDCORE_BRAVE_STORY_ROW_SET_SCRIPT_VARIABLES_03C0_03FF = 2,
    WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS = 3,
    WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS = 4,
    WLDCORE_BRAVE_STORY_ROW_TREASURES = 5,
    WLDCORE_BRAVE_STORY_ROW_LEAVE = 6,
} wldcore_brave_story_row_e;

/* Row kinds of the tutorial category list (wldcore_list_open_tutorial_categories). */
typedef enum wldcore_tutorial_category_row {
    WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION = 0,
    WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES = 1,
    WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS = 2,
    WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE = 3,
} wldcore_tutorial_category_row_e;

/* Row kinds of the brave-story proposition category list
 * (wldcore_menu_push_proposition_category_level). */
typedef enum wldcore_proposition_category_row {
    WLDCORE_PROPOSITION_CATEGORY_ROW_ACTIVE = 0,
    WLDCORE_PROPOSITION_CATEGORY_ROW_BRAVE_STORY = 1,
} wldcore_proposition_category_row_e;

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

extern s32 g_wldcore_bar_menu_saved_cursor;
extern u8 g_wldcore_bar_row_help_text_ids[];
void wldcore_bar_active_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_bar_build_menu_rows(wldcore_menu_list_window_level_t* level);
void wldcore_bar_push_rumor_detail_level(s32 value);
void wldcore_bar_push_menu_level(void);

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

extern menu_cursor_state_t g_wldcore_tutorial_entries_cursor_state;
extern s32 g_wldcore_tutorial_category_saved_cursor;
extern s32 g_wldcore_tutorial_event_ids[];
/* Per-row flag column of the world list panels; the list steps compare it
 * signed, and the two writers that only store into it agree on the width. */
extern s16 g_wldcore_list_row_flags[];

/* Row value column of the world list panels, read signed as `value & 0x7ff`. */
extern s16 g_wldcore_list_entry_values[];
void wldcore_list_open_script_flags_01a4_01bb(void);
void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel);
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible);
void wldcore_list_clear_cursor_state(s32 index);
void wldcore_list_open_tutorial_topics(void);
void wldcore_list_store_cursor_state(s32 index);

/* The list-level steps hand their menu level to these stubs, which read no
 * argument; the parameter is the argument the target passes in $a0. */
void wldcore_list_rumors_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_rumors_close_level(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_close_level(wldcore_menu_panel_level_t* level);
void wldcore_list_unexplored_lands_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_story_events_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_script_variables_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_tutorial_topics_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_formation_tutorials_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_script_flags_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_open_available_propositions(void);
void wldcore_list_open_completed_propositions(void);
void wldcore_list_open_propositions(void);
void wldcore_list_open_rumors(void);
void wldcore_list_open_tutorial_mask_1_entries(void);
void wldcore_list_open_formation_tutorials(void);
void wldcore_list_tutorial_mask_1_block_all_buttons(wldcore_menu_panel_level_t* unused_level);
void wldcore_list_open_story_events(void);
void wldcore_list_open_unexplored_lands(void);
void wldcore_list_open_treasures(void);
void wldcore_list_open_set_script_variables_03c0_03ff(void);
void wldcore_list_open_active_propositions(void);
void wldcore_list_open_brave_story_propositions(void);

void wldcore_list_open_tutorial_categories(s32 selected_entry, s32 reset);

/* camera */
/* Map camera block: translation, scale and rotation are passed together. */
typedef struct wldcore_map_projection_view {
    VECTOR origin;    /* 0x00 */
    VECTOR scale;     /* 0x10 */
    SVECTOR rotation; /* 0x20 */
} wldcore_map_projection_view_t;
typedef char wldcore_map_projection_view_size_must_be_40[sizeof(wldcore_map_projection_view_t) == 40 ? 1 : -1];

/* Packed eight-byte source row used by wldcore_map_color_and_draw_dots.
 * The low 16 bits of the first 32-bit value hold x; its high bytes select the animation. */
typedef struct wldcore_map_dot_source {
    s32 position_and_kind;
    s32 y;
} wldcore_map_dot_source_t;
typedef char wldcore_map_dot_source_size_must_be_8[sizeof(wldcore_map_dot_source_t) == 8 ? 1 : -1];

extern wldcore_map_dot_source_t* g_wldcore_map_dot_sources;

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

/* wldcore_projection_state_t.flags: work pending for
 * wldcore_map_update_camera_direction_and_projection, and the marker's sprite set. */
typedef enum wldcore_projection_flags {
    WLDCORE_PROJECTION_FLAG_UPDATE_POSITION = 0x01,
    WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION = 0x02,
    WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET = 0x04, /* set while the marker travels a route */
} wldcore_projection_flags_e;

/* The world map camera projection state at 0x8009f24c: two 32-bit control fields
 * followed by the player's own map dot. The marker's +8 base is what makes
 * the views of this address agree field for field -- the camera update
 * writes the dot's render sequence and screen position and the marker init writes
 * its kind, position and flags. For the player's marker, kind is the current
 * location id and sub_kind the proposition picture id. */
typedef struct wldcore_projection_state {
    s32 flags; /* 0x00; wldcore_projection_flags_e */
    s32 angle;
    wldcore_map_dot_t marker;
} wldcore_projection_state_t;

/* wldcore_projection_motion_t.flags. */
typedef enum wldcore_projection_motion_flag {
    WLDCORE_PROJECTION_MOTION_ACTIVE = 0x01,
    WLDCORE_PROJECTION_MOTION_ZOOMED_OUT = 0x02, /* g_wldcore_map_zoom_motion: the wide view, or the move to it */
} wldcore_projection_motion_flag_e;

/* Wrapped projection move: the origin and final target are copied as VECTORs. */
typedef struct wldcore_projection_motion {
    s32 flags; /* 0x00; wldcore_projection_motion_flag_e */
    s32 distance;
    s32 progress;
    VECTOR origin;
    VECTOR target;
} wldcore_projection_motion_t;
typedef char wldcore_projection_motion_size_must_be_44[sizeof(wldcore_projection_motion_t) == 44 ? 1 : -1];

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

/* wldcore_projected_entry_t.flags. */
typedef enum wldcore_map_tile_flag {
    WLDCORE_MAP_TILE_TPAGE_MASK = 0x0ff,
    WLDCORE_MAP_TILE_OFF_SCREEN = 0x100, /* set by the projection pass */
    WLDCORE_MAP_TILE_EMPTY = 0x200,      /* the atlas has no tile here */
} wldcore_map_tile_flag_e;

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

extern s32 g_wldcore_map_dot_count;
extern wldcore_map_dot_t g_wldcore_map_dots[];
extern wldcore_projection_state_t g_wldcore_map_projection_state;
extern wldcore_projection_motion_t g_wldcore_map_projection_motion;
extern wldcore_projected_entry_t g_wldcore_map_projected_tiles[];

/* Pointer to the 48 route polylines (array at 0x80095fb0), one per route;
 * each is a packed s16 count followed by that many (x, y) pairs (0x80096070
 * holds count 12 and the next record starts at 0x800960a2). A sibling of
 * g_wldcore_map_route_tables, indexed by the same route id. */
extern u16** g_wldcore_map_route_polylines;

/* Pointer cell at 0x80095000; the 48 route-record pointers begin at 0x80095008. */
extern wldcore_route_record_t** g_wldcore_map_route_tables;

/* The horizontal and vertical map scroll axes. */
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_x;
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_y;

/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both 32-bit values as one point32_t. */
extern point32_t g_wldcore_context_value_display_position;
extern u8 g_wldcore_map_dot_pulse_direction;
extern u8 g_wldcore_map_dot_pulse_phase;
extern wldcore_slideshow_t g_wldcore_map_path_animation;
extern s32 g_wldcore_map_window_index;
extern wldcore_map_projection_view_t g_wldcore_map_projection_view;

extern VECTOR g_wldcore_map_projection_origin;
extern SVECTOR g_wldcore_map_projection_rotation;
extern VECTOR g_wldcore_map_projection_scale;
extern s32* g_wldcore_map_tile_descriptors;

/* Provisional: the zoom animation record at 0x800d3c8c; flags bit 0 runs
 * the animation, bit 1 selects the zoomed-out direction. */
extern wldcore_zoom_motion_t g_wldcore_map_zoom_motion;

/* Indexed so the store in wldcore_location_process_entry stays behind the
 * window-flag store (MEM_IN_STRUCT_P). */
extern s32 g_wldcore_next_map_id[];
void wldcore_map_init_tile_atlas(void);
void wldcore_map_color_and_draw_dots(void);
s32 wldcore_map_build_location_menu_entries(s32 proposition_id, s32* out);
void wldcore_map_draw_visible_routes(GsOT* ot);
void wldcore_map_draw_path_animation(GsOT* ot);
void wldcore_map_find_best_route_path(s32 from, s32 to);
void wldcore_map_step_route_travel(void);
s32 wldcore_map_step_projection_motion(void);
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position);
void wldcore_map_apply_clamped_scroll(point32_t* amount, VECTOR* position);
void wldcore_map_apply_clamped_vertical_scroll(point32_t* amount, VECTOR* position);
s32 wldcore_map_find_dot_at_point(point32_t point);
void wldcore_map_init_location_marker_and_camera(void);
s32 wldcore_map_is_point_outside_projection_bounds(const point32_t* point);
void wldcore_map_negate_and_clamp_coordinates(const SVECTOR* input, VECTOR* output);
void wldcore_map_project_and_cull_dots(void);
void wldcore_map_project_and_cull_tiles(void);
void wldcore_map_pulse_dot_colors(void);
s32 wldcore_map_start_projection_motion_if_outside_bounds(const point32_t* point, const SVECTOR* coordinates);
void wldcore_map_update_camera_direction_and_projection(void);
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction);
s32 wldcore_map_roll_random_encounter(s32 location, s32 route);
void wldcore_map_scroll_projection_if_focus_near_edge(point32_t* delta);
void wldcore_push_map_location_visibility_level(s32 value, s32 flag);
void wldcore_map_prepare_path_animation(s32 from, s32 to);
s32 wldcore_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y);

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

/* Provisional: 16-byte rows of the window-layout table at 0x8009e93c.
 * wldcore_menu_handle_layout_window_input reads the first ten bytes: a pointer
 * to the level's live counter, the base message id, and the four neighboring
 * layout ids reached by the directional pad. wldcore_window_apply_layout reads
 * the trailing 16-bit values: the record's x and y extents (plus 14 and 12) and the
 * kind that selects the redraw delay. */
typedef struct wldcore_window_layout_row {
    s16* counter;     /* 0x00 */
    u16 message_base; /* 0x04 */
    u8 up;            /* 0x06 */
    u8 down;          /* 0x07 */
    u8 left;          /* 0x08 */
    u8 right;         /* 0x09 */
    s16 x;            /* 0x0a */
    s16 y;            /* 0x0c */
    s16 kind;         /* 0x0e */
} wldcore_window_layout_row_t;
typedef char wldcore_window_layout_row_size_must_be_16[sizeof(wldcore_window_layout_row_t) == 16 ? 1 : -1];

/* Message menu level pushed by 0x8006e860. */
typedef struct wldcore_menu_message_level {
    s32 reload_text;
    s32 saved_blocked_buttons; /* 0x04; g_wldcore_menu_blocked_buttons at push time */
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
/* World-map routes: g_wldcore_map_route_tables and the flags from
 * EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST. */
enum { WLDCORE_ROUTE_COUNT = 48 };

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

/* wldcore_menu_sound_novel_history_level_t.phase. */
typedef enum wldcore_sound_novel_history_phase {
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE = 0,
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM = 1,     /* five steps: the picture darkens under the fill box */
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE = 2, /* the reverse ramp, then the level pops */
} wldcore_sound_novel_history_phase_e;

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
typedef char
    wldcore_menu_location_transition_level_size_must_be_92[sizeof(wldcore_menu_location_transition_level_t) == 92 ? 1
                                                                                                                  : -1];

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

/* wldcore_menu_sound_novel_level_t.phase. */
typedef enum wldcore_sound_novel_phase {
    WLDCORE_SOUND_NOVEL_PHASE_RUN = 0,          /* the script runs */
    WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY = 2, /* square: counts down, then pushes the history level */
    WLDCORE_SOUND_NOVEL_PHASE_END = 3,          /* the script ended: waits for thread 14, then leaves */
} wldcore_sound_novel_phase_e;

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

/* Menu-stack handler selectors. Push routines supply the level record;
 * RETURN bridges the entry window and a pending world-map transition. */
typedef enum wldcore_menu_level_type {
    WLDCORE_MENU_LEVEL_ROOT = 0,
    WLDCORE_MENU_LEVEL_RETURN = 1,
    WLDCORE_MENU_LEVEL_TEXT_WINDOW = 2,
    WLDCORE_MENU_LEVEL_MESSAGE = 3,
    WLDCORE_MENU_LEVEL_LOCATION_MENU = 4,
    WLDCORE_MENU_LEVEL_BAR_MENU = 5,
    WLDCORE_MENU_LEVEL_RUMORS = 6,
    WLDCORE_MENU_LEVEL_AVAILABLE_PROPOSITIONS = 7,
    WLDCORE_MENU_LEVEL_RUMOR_DETAIL = 8,
    WLDCORE_MENU_LEVEL_PROPOSITION_DETAIL = 9,
    WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT = 10,
    WLDCORE_MENU_LEVEL_VARIABLE_LIST = 11,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL = 12,
    WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM = 13,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS = 14,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE = 15,
    WLDCORE_MENU_LEVEL_PROPOSITION_REPORT = 16,
    WLDCORE_MENU_LEVEL_PROPOSITION_RESULT = 17,
    WLDCORE_MENU_LEVEL_PROPOSITIONS = 18,
    WLDCORE_MENU_LEVEL_PROPOSITION_CANCEL_CONFIRM = 19,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME = 20,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL = 21,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT = 22,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY = 23,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LANDS = 24,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL = 25,
    WLDCORE_MENU_LEVEL_TREASURES = 26,
    WLDCORE_MENU_LEVEL_TREASURE_DETAIL = 27,
    WLDCORE_MENU_LEVEL_BRAVE_STORY = 28,
    WLDCORE_MENU_LEVEL_STORY_EVENTS = 29,
    WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT = 30,
    WLDCORE_MENU_LEVEL_SET_SCRIPT_VARIABLES_03C0_03FF = 31,
    WLDCORE_MENU_LEVEL_VARIABLE_DETAIL = 32,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS = 33,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT = 34,
    WLDCORE_MENU_LEVEL_EVENT_TRANSITION = 35,
    WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT = 36,
    WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY = 37,
    WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS = 38,
    WLDCORE_MENU_LEVEL_PARTICIPANT_LIST = 39,
    WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL = 40,
    WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS = 41,
    WLDCORE_MENU_LEVEL_PROPOSITION_ATTEMPT_DETAIL = 42,
    WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES = 43,
    WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS = 44,
    WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES = 45,
    WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB = 46,
    WLDCORE_MENU_LEVEL_SCRIPT_FLAG_01A4_DETAIL = 47,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT = 48,
    WLDCORE_MENU_LEVEL_SCREEN_TRANSITION = 49,
    WLDCORE_MENU_LEVEL_FOCUS_LOCATION = 50,
    WLDCORE_MENU_LEVEL_SCROLLABLE_TEXT_WINDOW = 51,
    WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL = 52,
    WLDCORE_MENU_LEVEL_MAP_PATH = 53,
    WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY = 54,
    WLDCORE_MENU_LEVEL_COUNTDOWN = 55,
    WLDCORE_MENU_LEVEL_LAYOUT_WINDOW = 56,
} wldcore_menu_level_type_e;

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

enum { WLDCORE_WINDOW_RECORD_CAPACITY = 32 };

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

/* Text width is in pixels; the second value counts visible list rows. */
typedef struct wldcore_window_panel_layout {
    s32 text_width;
    s32 visible_rows;
} wldcore_window_panel_layout_t;

extern s16 g_wldcore_panel_row_actions[16];
extern menu_cursor_state_t g_wldcore_menu_cursor_states[];

/* libgs sprite sorted by wldcore_draw_close_indicator_if_pending through
 * GsSortSprite (0x800e0480). */
extern GsSPRITE g_wldcore_menu_indicator_sprite;
/* Pad buttons withheld from the WORLD menu frame (wldcore_menu_run_world_frame passes its
 * complement to world_formation_set_menu_allowed_buttons); -1 blocks all. */
extern s32 g_wldcore_menu_blocked_buttons;
extern s32 g_wldcore_menu_stack_depth;
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records[];
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records_next[];
extern s32 g_wldcore_menu_stack_types[];
extern s32 g_wldcore_menu_stack_types_next[];
extern s32 g_wldcore_window_aux_render_object_count;
extern void* g_wldcore_window_aux_render_object_queue[];

/* Shared panel renderer state at 0x800bb31c, filled by
 * wldcore_window_init_panel_render_state and
 * wldcore_window_init_number_panel_render_thread and read by
 * world_screen_build_at_list. Its text binding is the label layout at 0x800bb394
 * (column mode 0 text, 2 hidden, 3 alternate number) or, for number panels,
 * g_wldcore_window_panel_number_state. */
extern menu_text_layout_t g_wldcore_window_panel_label_state;
extern menu_entry_t g_wldcore_window_panel_render_state;
extern s32 g_wldcore_window_record_count;
extern world_animated_render_record_t g_wldcore_window_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern wldcore_window_layout_row_t g_wldcore_window_layout_rows[];
extern s32 g_wldcore_window_render_object_count;

/* Display-object queues drawn by 0x8006ae20 each frame: every entry points at
 * the leading 32-bit flags of a window record, render record or map dot, and
 * wldcore_window_set_render_objects_visible toggles bit 0x08 through it. */
extern void* g_wldcore_window_render_object_queue[];
extern wldcore_window_render_record_t g_wldcore_window_render_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern s32 g_wldcore_window_render_record_count;
extern wldcore_window_xy_t g_wldcore_window_record_positions[];

extern u32 g_wldcore_window_image_buffer[];
extern s16 g_wldcore_active_menu_value;
extern void (*g_wldcore_menu_idle_handlers[])(wldcore_menu_stack_record_t*);
extern s32 g_wldcore_menu_ordering_table_offset;
extern void (*g_wldcore_menu_publish_handlers[])(wldcore_menu_stack_record_t*);
extern void (*g_wldcore_menu_resume_handlers[])(wldcore_menu_stack_record_t*);
extern struct world_menu_number_range g_wldcore_window_panel_number_state;
extern s32 g_wldcore_menu_root_window_index;
void wldcore_window_init_record_pools(void);
void wldcore_window_build_yes_no_panel(point32_t origin);
void wldcore_menu_push_location_menu_level(s32 menu);
void wldcore_menu_pop_level_and_rebuild_screen(wldcore_menu_list_window_level_t* unused_level);
void wldcore_menu_push_event_transition_level(s32 mode);
void wldcore_draw_close_indicator_if_pending(GsOT* ot);
void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore);
void wldcore_menu_dispatch_idle_handler(void);
void wldcore_menu_dispatch_pending_script_requests(void);
void wldcore_menu_dispatch_publish_handler(void);
void wldcore_menu_dispatch_resume_handler(void);
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_handle_story_event_text_input(wldcore_menu_stack_record_t* level);
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level);

/* Menu resume callbacks consume the variant selected by the level tag. */
void wldcore_window_clear_layout_states(wldcore_menu_stack_record_t* level);
void wldcore_window_restore_proposition_detail_palette(wldcore_menu_stack_record_t* level);
void wldcore_window_reload_data_set_and_resume_thread14(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_pair_state(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_slot_state(wldcore_menu_stack_record_t* level);
void wldcore_window_restore_location_list_palette(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_layout_states_and_queue_msg_b806(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_layout_states_and_queue_msg_b8dd(wldcore_menu_stack_record_t* level);
void wldcore_window_finalize_record_and_stop_task(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_root_level(wldcore_menu_stack_record_t* level);

void wldcore_menu_display_text_entry(s32 index, s32 text_id, point32_t point, u32* buffer);
void wldcore_menu_push_message_level(s32 text, s32 reload_text);
void wldcore_menu_push_fixed_number_panel_level(void);
void wldcore_menu_push_number_panel_level_with_argument(s32 variable_id, s32 value);
void wldcore_menu_push_root_level(void);
void wldcore_menu_reset_window_list_and_store_origin(void);
void wldcore_menu_restore_level_on_world_entry(void);
void wldcore_menu_run_world_frame(GsOT* context);
void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown);

/* Both return the appended record's index. */
s32 wldcore_window_append_record_and_reset_color(void** queue, s32* count);
s32 wldcore_window_append_render_record_and_reset_color(void** queue, s32* count);
void wldcore_window_apply_layout(wldcore_window_layout_request_t* request);

void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);

void wldcore_window_draw_centered_text(s32 window_index, s32 text_id);
void wldcore_window_draw_indexed_text(wldcore_menu_sound_novel_history_level_t* level, s32 index);
void wldcore_window_draw_text_rows(wldcore_menu_send_unit_level_t* level);

void wldcore_window_init_number_panel_render_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin);

void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);

void wldcore_window_init_record(s32 index, s32 priority, s32 sequence);

void wldcore_window_init_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image);

void wldcore_window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags);

void wldcore_window_load_image_record_to_vram(s32 index, u32* image);
void wldcore_window_publish_slot_b806(wldcore_menu_list_window_level_t* level);
void wldcore_window_publish_slot_b8dd(wldcore_menu_list_window_level_t* level);
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_stack_record_t* level, s32 proposition, s32 coordinate_mode);
void wldcore_proposition_handle_cancel_confirm_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_handle_participant_list_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_step_map_cursor_level(wldcore_menu_stack_record_t* level);
void wldcore_window_reset_state_and_scroll(wldcore_menu_stack_record_t* level);
void wldcore_map_step_projection_motion_then_pop_level(wldcore_menu_stack_record_t* entry);
void wldcore_window_set_render_objects_visible(s32 clear);
void wldcore_window_set_render_palette_2(wldcore_window_render_pair_t* pair);

void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);

void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output);
void wldcore_window_set_layout_states(wldcore_menu_proposition_list_level_t* level);
void wldcore_menu_push_participant_list_level(s32 proposition);
void wldcore_menu_push_proposition_attempt_detail_level(s32 value);
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b);
void wldcore_menu_push_script_flag_01a4_detail_level(s32 value);
void wldcore_menu_push_variable_detail_level(s32 value);
void wldcore_menu_push_story_event_text_level(s32 value);
void wldcore_menu_push_treasure_detail_level(s32 value);
void wldcore_menu_push_entry_window_level(void);
void wldcore_menu_push_tutorial_topic_text_level(s32 value);
void wldcore_menu_push_unexplored_land_detail_level(s32 value);
void wldcore_menu_push_focus_location_level(s32 id);
void wldcore_menu_push_map_path_level(s32 value, s32 value_hi, s32 flag);
void wldcore_window_set_list_window_states(wldcore_menu_list_window_level_t* state);
void wldcore_menu_push_proposition_category_level(void);
void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level);
void wldcore_menu_push_layout_window_level(void);
void wldcore_menu_push_participant_detail_level(s32 proposition, s32 participant);
void wldcore_window_end_tint_transition(wldcore_menu_list_window_level_t* level);
void wldcore_window_begin_tint_transition(wldcore_menu_list_window_level_t* level);
void wldcore_menu_push_proposition_report_level(s32 argument);
void wldcore_menu_build_brave_story_entries(void);
void wldcore_menu_push_brave_story_level(void);
void wldcore_window_close_parent_list_all_windows(void);
void wldcore_window_close_parent_list_frame_windows(void);
void wldcore_menu_push_scrollable_text_window_level(s32 text_id);
void wldcore_menu_run_shop_and_rebuild_screen(s32 shop_id);
void wldcore_window_set_secondary_render_palette_2(wldcore_menu_slot_set_t* slots);
void wldcore_menu_push_variable_list_level(void);
void wldcore_window_load_party_unit_sprite_record(wldcore_menu_send_unit_level_t* level, s32 unit, s32 render_index);
void wldcore_menu_push_number_panel_level(void);
void wldcore_window_clear_states_and_restore_tint(wldcore_menu_send_unit_level_t* level);
void wldcore_window_set_states_and_blue_tint(wldcore_menu_send_unit_level_t* level);
void wldcore_menu_push_sound_novel_quit_level(void);
void wldcore_menu_push_sound_novel_history_level(void);
void wldcore_window_reset_slot_state(wldcore_menu_save_confirm_level_t* level);
void wldcore_menu_push_text_window_level(s32 text, s32 reload_text);
void wldcore_menu_push_sound_novel_level(s32 mode);
void wldcore_menu_push_sound_novel_resume_level(s32 mode);
void wldcore_menu_push_countdown_level(void);

void wldcore_window_number_panel_noop_callback(void);
/* bits */

void wldcore_set_bit_value(u32* bits, s32 index, s32 set);
s32 wldcore_test_bit(u32* bits, s32 index);

void wldcore_write_nine_bit_record(u32* bits, s32 record, calendar_date_t* source);
void wldcore_read_nine_bit_record(u32* bits, s32 record, calendar_date_t* dest);

#endif

#ifndef SAVE_H
#define SAVE_H

/* Saving and loading: MAIN's play time, save image and slot table, CARD's save menu in battle and WORLD's memory card
 * screen, with the globals both of those define; the card BIOS layer is in memcard.h. */

#include "fft/calendar.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/option.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/kernel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* play time */
extern s32 g_save_play_time_frames;
extern s32 g_save_play_time_hours;
extern s32 g_save_play_time_minutes;
extern s32 g_save_play_time_seconds;

/* save image */
/* PlayStation memory-card save-file header block (256 bytes), as built by save_build_file_header, which CARD and
 * WORLD each define. */
typedef struct save_file_header {
    u8 magic0;           /* 0x00: 'S' */
    u8 magic1;           /* 0x01: 'C' */
    u8 icon_flags;       /* 0x02: 0x11, one icon frame */
    u8 block_count;      /* 0x03 */
    u8 title[0x40];      /* 0x04: Shift-JIS save-file title */
    u8 reserved[0x1C];   /* 0x44 */
    palette_t icon_clut; /* 0x60 */
    u8 icon_image[0x80]; /* 0x80 */
} save_file_header_t;

/* The fixed leading bytes of save_file_header_t. */
enum {
    SAVE_HEADER_MAGIC_S = 'S',
    SAVE_HEADER_MAGIC_C = 'C',
    SAVE_HEADER_ICON_ONE_FRAME = 0x11,
    SAVE_HEADER_BLOCK_COUNT_ONE = 1,
};

/* What a save slot's line on the save screen shows: the first 0x18 bytes of a
 * save's data block. g_save_slot_metadata keeps one per slot. */
typedef struct save_summary {
    u8 slot;                 /* 0x00; save slot, 0xff while the buffer is incomplete */
    u8 name[UNIT_NAME_SIZE]; /* 0x01; save-description leader name, 0xfe-terminated */
    u8 name_terminator;      /* 0x11 */
    u8 job_id;               /* 0x12 */
    u8 level;                /* 0x13 */
    calendar_date_t date;    /* 0x14; month/day from script variables 0x2e/0x2f */
    u8 location;             /* 0x16; script variable 0x31, location text index */
    u8 format_version;       /* 0x17; CARD writes 4 and WORLD load requires 4 */
} save_summary_t;
typedef char save_summary_size_must_be_24[(sizeof(save_summary_t) == 24) ? 1 : -1];

/* The 0x1e00-byte memory-card save image, parity-covered per 128-byte block.
 * WORLD builds and loads it in g_save_work_buffer; CARD fills the same
 * layout from the SCUS globals in save_build_image through
 * g_save_buffer_pointer. */
typedef struct save_image {
    save_file_header_t header; /* 0x000 */
    save_summary_t summary;    /* 0x100 */
    u8 parity_bits[8];         /* 0x118; one parity bit per 128-byte block */
    s32 elapsed_seconds;       /* 0x120 */
    /* 0x124-0x1dbe: filled by CARD's and WORLD's save_build_image from the SCUS globals named in their bcopy
     * calls. */
    u8 treasure_acquisition_date_bits[0x35];     /* 0x124; 47 packed 9-bit dates and 1 spare bit */
    u8 land_discovery_date_bits[0x12];           /* 0x159; packed 9-bit discovery dates */
    u8 proposition_last_attempt_date_bits[0x6c]; /* 0x16b; packed 9-bit dates */
    u8 proposition_states[0x60];                 /* 0x1d7; low 6 bits are a remaining-day counter */
    u8 _unused_0237[1];                          /* 0x237; serialized and parity-covered */
    u8 sound_novel_saved_records[5][0x38];       /* 0x238; sound_novel_reading_t: one sound-novel reading state each */
    u32 sound_novel_read_section_bits[40];       /* 0x350 */
    u32 sound_novel_saved_flags[2];              /* 0x3f0 */
    u8 brave_story_character_ages[0x40];         /* 0x3f8 */
    u8 proposition_count;                        /* 0x438 */
    u8 active_propositions[8][9];                /* 0x439 */
    u8 _unused_0481[3];                          /* 0x481; serialized and parity-covered */
    u8 party_records[PARTY_ROSTER_SLOT_COUNT][0xe0]; /* 0x484; party_unit_t records truncated to 0xe0 bytes */
    u8 item_quantities[ITEM_ID_COUNT];               /* 0x1604; g_item_quantities */
    u8 poached_item_quantities[ITEM_ID_COUNT];       /* 0x1704; g_item_poached_quantities */
    /* 0x1804; first 0x40 bytes are Move-Find-Item flags, second half unknown. */
    u8 item_location_flags[0x80];
    s32 script_variables[0x100];   /* 0x1884 */
    game_options_fields_t options; /* 0x1c84; copied to g_game_options by save_restore_image */
    u8 _unknown_1c88;              /* 0x1c88 */
    s8 item_type_order_0[0xc];     /* 0x1c89; signed, -1-terminated */
    u8 _unused_1c95[1];            /* 0x1c95; serialized and parity-covered */
    s8 item_type_order_1[8];       /* 0x1c96; signed, -1-terminated */
    u8 _unused_1c9e[1];            /* 0x1c9e; serialized and parity-covered */
    s8 item_type_order_2[7];       /* 0x1c9f; signed, -1-terminated */
    s8 item_type_order_3[5];       /* 0x1ca6; signed, -1-terminated */
    s8 item_type_order_4[5];       /* 0x1cab; signed, -1-terminated */
    s8 item_type_order_5[7];       /* 0x1cb0; signed, -1-terminated */
    s8 item_type_order_6[5];       /* 0x1cb7; signed, -1-terminated */
    u8 weapon_page_order[0x8a];    /* 0x1cbc */
    u8 helmet_page_order[0x1d];    /* 0x1d46 */
    u8 armor_page_order[0x25];     /* 0x1d63 */
    u8 accessory_page_order[0x21]; /* 0x1d88 */
    u8 item_page_order[0x15];      /* 0x1da9 */
    u8 _unused_1dbe[0x42];         /* 0x1dbe; parity-covered tail */
} save_image_t;
typedef char save_image_size_must_be_7680[(sizeof(save_image_t) == 7680) ? 1 : -1];

/* The 0x1e00-byte serialized save is parity-covered in 0x80-byte blocks;
 * each block contributes 0x400 one-bit reads to the primary parity stream. */
enum {
    SAVE_PARITY_BLOCK_COUNT = 60,
    SAVE_PARITY_BITS_PER_BLOCK = 1024,
    SAVE_PARITY_BLOCK_BYTES = 128,
};

/* One selectable save-file icon: 16-color CLUT plus 16x16 4bpp image. */
typedef struct save_icon {
    palette_t clut; /* 0x00 */
    u8 image[0x80]; /* 0x20 */
} save_icon_t;

extern u8 g_save_unread_byte; /* saved and restored with save image offset 0x1c88; never read */

void save_init_data_tables(void);
void save_init_state_and_options(void);

/* save slots */
/* The scanner initializes 15 file states, then allocates available card
 * blocks to usable new-save entries. State 0 means a readable save header. */
typedef enum save_slot_state {
    SAVE_SLOT_UNSCANNED = -2,
    SAVE_SLOT_UNAVAILABLE = -1,
    SAVE_SLOT_OCCUPIED = 0,
    SAVE_SLOT_AVAILABLE = 1
} save_slot_state_e;

enum { SAVE_SLOT_COUNT = 15 };

/* The value of g_save_last_used_slot when no slot was loaded or written. */
enum {
    SAVE_LAST_USED_SLOT_NONE = 0xff,
};

/* Hours, minutes and seconds shown for one memory-card save slot. */
typedef struct save_play_time {
    u8 hours;
    u8 minutes;
    u8 seconds;
} save_play_time_t;
typedef char save_play_time_size_must_be_3[(sizeof(save_play_time_t) == 3) ? 1 : -1];

extern u8 g_save_last_used_slot; /* the save slot last loaded or written */
extern u8 g_save_selected_slot;
extern s8 g_save_slot_file_states[]; /* save_slot_state_e */
extern save_summary_t g_save_slot_metadata[SAVE_SLOT_COUNT];
extern save_play_time_t g_save_slot_playtimes[SAVE_SLOT_COUNT];

/* shared by CARD and WORLD */
extern save_icon_t g_save_icon_records[15];
extern u8 g_save_title_template[0x16];
extern const char* g_save_file_names[15];
extern menu_window_command_t g_save_slot_window_command;
extern s8 g_save_free_block_count;
extern s8 g_save_file_count;
extern s8 g_save_slot_scan_active;
extern s8 g_save_slot_scan_index;
extern s8 g_save_format_delay_counter;
extern u8 g_save_write_failed;

/* Encoded description text per memory-card slot: CARD keeps two lines of 80
 * bytes for each slot, WORLD 140 bytes. */
#ifdef FFT_WORLD
extern u8 g_save_slot_descriptions[][0x8C];
#else
extern u8 g_save_slot_descriptions[SAVE_SLOT_COUNT][0x50];
#endif

void save_build_image(s32 slot);

/* CARD save menu */
/* g_save_menu_state, the state card_entrypoint dispatches on. Only
 * the states whose roles the matching handlers establish are named. */
typedef enum save_menu_state {
    SAVE_MENU_INITIAL_PROMPT = 0,
    SAVE_MENU_SELECT_CARD = 1,
    SAVE_MENU_SCAN_RESULT = 2,
    SAVE_MENU_SCANNING = 3,
    SAVE_MENU_SELECT_SAVE = 4,
    SAVE_MENU_WRITING = 6,
    SAVE_MENU_LEAVE_ALT = 9, /* never stored; handled like LEAVE */
    SAVE_MENU_LEAVE = 10,    /* the player declined or is done: fade the music and the screen out */
    SAVE_MENU_CLOSING = 0xff /* fading out; no state handler runs */
} save_menu_state_e;

/* 0x1e80 bytes running to g_save_open_result: the 0x1e00-byte save image plus a 0x80-byte tail that
 * save_init_menu_state clears with it. */
extern save_image_t g_save_buffer[];
extern save_image_t* g_save_buffer_pointer;
extern u8 g_save_menu_state; /* save_menu_state_e */
extern DIRENTRY* g_save_menu_work_buffer;
extern clut_t g_save_menu_clut;
extern s32 g_save_menu_script_callback;
extern s16 g_save_menu_selection_index;
extern s8 g_save_completed;

/* Run CARD's save-menu loop until it closes. */
void card_entrypoint(s32 unused);
void save_backdrop_thread(void);

/* CARD prompts */
/* The g_save_*_descriptor menu-thread descriptors are 0x3c-byte menu_entry_t menu records. menu_start_managed_thread
 * and its delayed twin start descriptor->thread_entry (+0x28). The yes/no prompts are a message record followed by a
 * choice record whose text_id is 0xd810; the choice's selected_index (+0x74 of the prompt) is the default answer,
 * and the message window.y (+0x0a) places the prompt. */
extern menu_entry_t g_save_initial_prompt_descriptor[];
extern menu_entry_t g_save_card_slot_selection_descriptor[];
extern menu_entry_t g_save_checking_message_descriptor[];
extern menu_entry_t g_save_card_error_prompt_descriptor[];
extern menu_entry_t g_save_no_data_prompt_descriptor[];
extern menu_entry_t g_save_format_prompt_descriptor[2];
extern menu_entry_t g_save_formatting_message_descriptor[];
extern menu_entry_t g_save_overwrite_prompt_descriptor[2];
extern menu_entry_t g_save_in_progress_thread_descriptor[];
extern menu_entry_t g_save_failure_thread_descriptor[];

/* A pointer cell, not a record: save_update_scan_result clears it to
 * 0, tests it, and assigns g_save_no_data_prompt_descriptor into it. */
extern menu_entry_t* g_save_slot_scan_prompt_descriptor;
extern s16 g_save_initial_prompt_timer;
extern u8 g_save_initial_prompt_thread_state;
extern u8 g_save_slot_selection_thread_state;
extern s8 g_save_error_prompt_closing;
extern s8 g_save_format_prompt_active;
extern u8 g_save_overwrite_prompt_active;

/* CARD scan and write */
extern s8 g_save_open_result;
extern s8 g_save_occupied_slot_count;
extern s8 g_save_scan_setup_step;
extern u8 g_save_write_phase;
extern u8 g_save_unread_flag; /* cleared with g_save_write_failed when a save starts; never read */

/* CARD slot list */
/* Ten-byte number command in a 12-byte save-menu slot. */
typedef struct save_slot_number_command {
    menu_draw_number_command_t number;
    u8 _unused_0a[2];
} save_slot_number_command_t;
typedef char save_slot_number_command_size_must_be_12[sizeof(save_slot_number_command_t) == 12 ? 1 : -1];

extern s16 g_save_slot_index;
extern s16 g_save_list_selected_slot;
extern s16 g_save_list_top_row;
extern u8 g_save_list_cursor_index;
extern s16 g_save_list_scroll_anim_offset;
extern s16 g_save_list_scroll_base_y;
extern s16 g_save_slot_row_y;
extern u16 g_save_slot_cursor_animation;
extern point16_t g_save_slot_cursor_position;
extern s16 g_save_slot_text_row_ids[];
extern save_slot_number_command_t g_save_slot_number_commands[4];
extern oriented_quad_t g_save_slot_quads[];
extern menu_window_rect_source_t g_save_slot_window_rect_source[];

/* CARD graphics */
/*
 * CARD.OUT's double-buffered graphics context. g_save_gfx_context_base points
 * at the two-element array and g_save_gfx_context at the active buffer.
 *
 * The 0xf4 stride is established twice by the matching code:
 * save_gfx_init_contexts steps its per-buffer offset by 0xf4, and
 * save_gfx_submit_frame toggles the active pointer by the
 * same 0xf4. The field offsets come from the accesses in those routines and
 * in the enqueue helpers: the ordering table at 0x00, the POLY_FT4 pool at
 * 0x10, the tile pool at 0x3c, and the Psy-Q environments at 0x60/0xbc/0xd0
 * (DRAWENV 92 + DISPENV 20 + DR_MODE 12 end exactly at 0xdc).
 */
typedef struct save_gfx_context {
    u32* otag;                   /* 0x00 */
    u8 _unused_04[0x0c];         /* 0x04 */
    POLY_FT4* polygons;          /* 0x10 */
    u8 _unused_14[0x28];         /* 0x14 */
    TILE* tiles;                 /* 0x3c: TILE pool */
    u8 _unused_40[0x20];         /* 0x40 */
    DRAWENV draw_environment;    /* 0x60 */
    DISPENV display_environment; /* 0xbc */
    DR_MODE draw_mode;           /* 0xd0 */
    u8 _unused_dc[0x18];         /* 0xdc */
} save_gfx_context_t;
typedef char save_gfx_context_size_must_be_244[(sizeof(save_gfx_context_t) == 244) ? 1 : -1];

extern save_gfx_context_t* g_save_gfx_context_base;
extern save_gfx_context_t* g_save_gfx_context;
extern u16 g_save_gfx_drawenv_y;
extern u16 g_save_gfx_texture_page;
extern u16 g_save_gfx_draw_clut;
extern rgb24_t g_save_gfx_fade_overlay_color;
extern RECT g_save_gfx_fullscreen_fade_rect;

void save_gfx_init_contexts(save_gfx_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles, s32 unused_17, s32 unused_18,
    s32 unused_19, s32 unused_20, s32 unused_21, s32 unused_22, s32 unused_23, s32 unused_24);
void save_gfx_set_render_otag(u32* otag);
void save_gfx_submit_frame(s32 first_otag, s32 draw_otag);

/* CARD input, text and sound */
extern zodiac_draw_context_t g_save_input_display_state;
extern u16 g_save_input_latched_button;
extern u16 g_save_input_previous;
extern u8 g_save_list_input_armed;
extern const char g_save_text_decimal_format[];
extern const u8 g_save_text_command_help[];
extern const u8 g_save_text_job_name[];
extern const u8 g_save_text_location_name[];
extern u8 g_save_sound_queued_effect_id; /* sound_sfx_e */

void save_update_input(void);

/* WORLD memory card screen */
/* Step of the WORLD memory card screen (save_run_screen). */
typedef enum save_step {
    SAVE_STEP_SELECT_CARD = 0, /* memory card slot 1 or 2 */
    SAVE_STEP_SELECT_MODE = 1, /* save or load */
    SAVE_STEP_SCANNING = 2,
    SAVE_STEP_SELECT_SAVE = 3, /* the save slot list takes input */
    SAVE_STEP_LOADING = 4,
    SAVE_STEP_SAVING = 5,
    SAVE_STEP_LEAVE = 9,   /* starts the fade out */
    SAVE_STEP_LEFT = 0xff, /* no step runs while the screen fades */
} save_step_e;

extern u8 g_save_menu_step;           /* save_step_e */
extern s16 g_save_menu_request_state; /* world_map_request_state_e */
extern u8 g_save_menu_result;
extern u8 g_save_mode;
extern s16 g_save_mode_select_enabled;
extern s16 g_save_active_slot;
extern RECT g_save_screen_background_tile;
extern menu_entry_t g_save_slot_select_menu[2];
extern u8 g_save_slot_select_running;
extern s16 g_save_slot_select_timer;
extern u8 g_save_slot_cursor_row;
extern menu_entry_t g_save_mode_select_menu;
extern u8 g_save_mode_select_running;
extern s8 g_save_scan_prompt_active;
extern s8 g_save_scan_start_frame;
extern s8 g_save_probe_result;
extern struct DIRENTRY* g_save_directory_buffer;
extern s8 g_save_selectable_slot_count;

void save_init_screen(void);
s32 save_run_screen(s32 mode_select_enabled);
void save_close_stub(void);

/* WORLD messages and prompts */
extern menu_entry_t g_save_saving_message;
extern menu_entry_t g_save_loading_message;
extern menu_entry_t g_save_checking_message;
extern menu_entry_t g_save_formatting_message;
extern menu_entry_t g_save_load_error_message;
extern menu_entry_t g_save_write_error_message;
extern menu_entry_t g_save_access_error_message;
extern s8 g_save_error_message_closing;

/* Message entries selected by save_mode_select_step. */
extern menu_entry_t g_save_unavailable_message;
extern menu_entry_t g_save_no_data_message;

/* A pointer cell, not a record: cleared to 0, tested, and assigned
 * g_save_unavailable_message / g_save_no_data_message by save_mode_select_step. */
extern menu_entry_t* g_save_pending_message;

/* Two-entry confirmation chains; the choice is entry 1 selected_index. */
extern menu_entry_t g_save_format_prompt[2];
extern menu_entry_t g_save_overwrite_confirm_prompt[2];
extern menu_entry_t g_save_load_confirm_prompt[2];
extern menu_entry_t* g_save_confirm_prompt;
extern u8 g_save_overwrite_prompt_running;
extern u8 g_save_load_prompt_running;
extern u8 g_save_load_step_state;
extern u8 g_save_write_step_state;
extern u8 g_save_load_failed;
extern u8 g_save_load_unread_flag;  /* cleared with g_save_load_failed when a card load starts; never read */
extern u8 g_save_write_unread_flag; /* cleared with g_save_write_failed when a save starts; never read */

/* WORLD slot list */
/* Encoded captions addressed directly by save_build_slot_description,
 * immediately after the 15 save-file pointers at g_save_file_names. */
extern u8 g_save_slot_text_table[];
extern s16 g_save_slot_description_row_ids[];
extern menu_window_rect_source_t g_save_slot_window;
extern menu_draw_number_command_t g_save_slot_number_command;
extern menu_draw_number_command_t g_save_playtime_hours_command;
extern menu_draw_number_command_t g_save_playtime_minutes_command;
extern menu_draw_number_command_t g_save_playtime_seconds_command;
extern oriented_quad_t g_save_slot_quad_0;
extern oriented_quad_t g_save_slot_quad_1;
extern oriented_quad_t g_save_slot_quad_2;
extern oriented_quad_t g_save_slot_quad_3;
extern oriented_quad_t g_save_slot_quad_4;
extern point16_t g_save_slot_cursor_point;
extern u16 g_save_slot_cursor_anim;
extern u8 g_save_slot_list_input_ready;
extern s16 g_save_list_scroll_y;
extern s16 g_save_list_scroll_velocity;
extern s16 g_save_list_first_visible_row;

/* WORLD save image */
extern save_image_t* g_save_work_buffer;

s32 save_read_file_chunked(const char* filename, u8* destination, u32 size);
s32 save_restore_image(s32 partial_load);

#endif

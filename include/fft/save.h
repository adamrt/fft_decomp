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
#include "fft/proposition.h"
#include "fft/sound_novel.h"
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
    SAVE_HEADER_BLOCK_COUNT_ONE = 1
};

/* What a save slot's line on the save screen shows: the first 0x18 bytes of a
 * save's data block. g_save_slot_metadata keeps one per slot. */
typedef struct save_summary {
    u8 slot;                     /* 0x00; save slot, 0xff while the buffer is incomplete */
    u8 name[UNIT_NAME_SIZE + 1]; /* 0x01; 16 copied name bytes and an appended 0xfe terminator */
    u8 job_id;                   /* 0x12 */
    u8 level;                    /* 0x13 */
    calendar_date_t date;        /* 0x14; month/day from script variables 0x2e/0x2f */
    u8 location;                 /* 0x16; script variable 0x31, location text index */
    u8 format_version;           /* 0x17; CARD writes 4 and WORLD load requires 4 */
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
    /* 0x238; complete reading states, including their serialized pointer words. */
    sound_novel_reading_t sound_novel_saved_records[5];
    u32 sound_novel_read_section_bits[40];           /* 0x350 */
    u32 sound_novel_saved_flags[2];                  /* 0x3f0 */
    u8 brave_story_character_ages[0x40];             /* 0x3f8 */
    u8 proposition_count;                            /* 0x438 */
    proposition_progress_t active_propositions[8];   /* 0x439 */
    u8 _unused_0481[3];                              /* 0x481; serialized and parity-covered */
    u8 party_records[PARTY_ROSTER_SLOT_COUNT][0xe0]; /* 0x484; party_unit_t records truncated to 0xe0 bytes */
    u8 item_quantities[ITEM_ID_COUNT];               /* 0x1604; g_item_quantities */
    u8 poached_item_quantities[ITEM_ID_COUNT];       /* 0x1704; g_item_poached_quantities */
    /* 0x1804; first 0x40 bytes are Move-Find-Item flags, second half unknown. */
    u8 item_location_flags[0x80];
    s32 script_variables[0x100];          /* 0x1884 */
    game_options_fields_t options;        /* 0x1c84; copied to g_game_options by save_restore_image */
    u8 _unknown_1c88;                     /* 0x1c88 */
    s8 unit_list_sort_order[0xc];         /* 0x1c89; signed, -1-terminated */
    u8 _unused_1c95[1];                   /* 0x1c95; serialized and parity-covered */
    s8 formation_sort_order[8];           /* 0x1c96; signed, -1-terminated */
    u8 _unused_1c9e[1];                   /* 0x1c9e; serialized and parity-covered */
    s8 weapons_and_shields_sort_order[7]; /* 0x1c9f; signed, -1-terminated */
    s8 headgear_sort_order[5];            /* 0x1ca6; signed, -1-terminated */
    s8 body_armor_sort_order[5];          /* 0x1cab; signed, -1-terminated */
    s8 accessories_sort_order[7];         /* 0x1cb0; signed, -1-terminated */
    s8 other_items_sort_order[5];         /* 0x1cb7; signed, -1-terminated */
    u8 weapon_page_order[0x8a];           /* 0x1cbc */
    u8 helmet_page_order[0x1d];           /* 0x1d46 */
    u8 armor_page_order[0x25];            /* 0x1d63 */
    u8 accessory_page_order[0x21];        /* 0x1d88 */
    u8 item_page_order[0x15];             /* 0x1da9 */
    u8 _unused_1dbe[0x42];                /* 0x1dbe; parity-covered tail */
} save_image_t;
typedef char save_image_size_must_be_7680[(sizeof(save_image_t) == 7680) ? 1 : -1];

/* The 0x1e00-byte serialized save is parity-covered in 0x80-byte blocks;
 * each block contributes 0x400 one-bit reads to the primary parity stream. */
enum { SAVE_PARITY_BLOCK_COUNT = 60, SAVE_PARITY_BITS_PER_BLOCK = 1024, SAVE_PARITY_BLOCK_BYTES = 128 };

/* One selectable save-file icon: 16-color CLUT plus 16x16 4bpp image. */
typedef struct save_icon {
    palette_t clut; /* 0x00 */
    u8 image[0x80]; /* 0x20 */
} save_icon_t;

/* Serialized at save-image offset 0x1c88 and restored by full loads and tutorial swaps.
 * Its purpose is unknown; the original MAIN and tutorial images contain 0xc5. */
extern u8 g_save_unknown_byte;

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
enum { SAVE_LAST_USED_SLOT_NONE = 0xff };

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
extern const char* g_save_file_names[15];
extern menu_window_command_t g_save_slot_window_command;
extern s8 g_save_free_block_count;
extern s8 g_save_file_count;
extern s8 g_save_slot_scan_active;
extern s8 g_save_slot_scan_index;
extern s8 g_save_format_delay_counter;
extern u8 g_save_write_failed;

/* Encoded two-line description per save slot: CARD reserves 80 bytes, WORLD 140. */
#ifdef FFT_WORLD
extern u8 g_save_slot_descriptions[SAVE_SLOT_COUNT][0x8C];
#else
extern u8 g_save_slot_descriptions[SAVE_SLOT_COUNT][0x50];
#endif

void save_build_image(s32 slot);

/* CARD save menu */
/* Run CARD's save-menu loop until it closes. */
void card_entrypoint(s32 unused);
void save_backdrop_thread(void);

/* CARD and WORLD each define the rectangle used for a save-slot row. */
extern menu_window_rect_source_t g_save_slot_window;

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
    POLY_FT4* textured_quads;    /* 0x10 */
    u8 _unused_14[0x28];         /* 0x14 */
    TILE* tiles;                 /* 0x3c: TILE pool */
    u8 _unused_40[0x20];         /* 0x40 */
    DRAWENV draw_environment;    /* 0x60 */
    DISPENV display_environment; /* 0xbc */
    DR_MODE draw_mode;           /* 0xd0 */
    u8 _unused_dc[0x18];         /* 0xdc */
} save_gfx_context_t;
typedef char save_gfx_context_size_must_be_244[(sizeof(save_gfx_context_t) == 244) ? 1 : -1];

extern save_gfx_context_t* g_save_gfx_context;
extern u16 g_save_gfx_drawenv_y;

void save_gfx_init_contexts(save_gfx_context_t* contexts, u32* otags, s32 unused_2, s32 unused_3, s32 unused_4,
    POLY_FT4* polygons, s32 unused_6, s32 unused_7, s32 unused_8, s32 unused_9, s32 unused_10, s32 unused_11,
    s32 unused_12, s32 unused_13, s32 unused_14, s32 unused_15, TILE* tiles, s32 unused_17, s32 unused_18,
    s32 unused_19, s32 unused_20, s32 unused_21, s32 unused_22, s32 unused_23, s32 unused_24);
void save_gfx_set_render_otag(u32* otag);
void save_gfx_submit_frame(s32 first_otag, s32 draw_otag);

/* CARD input, text and sound */
void save_update_input(void);

/* WORLD memory card screen */
void save_init_screen(void);
s32 save_run_screen(s32 mode_select_enabled);
void save_close_stub(void);

/* WORLD save image */
extern save_image_t* g_save_work_buffer;

s32 save_read_file_chunked(const char* filename, u8* destination, u32 size);
s32 save_restore_image(s32 partial_load);

#endif

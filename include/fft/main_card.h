#ifndef MAIN_CARD_H
#define MAIN_CARD_H

/* Memory-card saves: the save-file image, the save-slot scan and the card BIOS events. */

#include "fft/data.h"
#include "fft/data_item.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_option.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* card events */
/* Memory-card BIOS event handles opened by main_card_init_events; the CARD,
 * OPEN and WORLD pollers test them by these indices. */
enum {
    MAIN_CARD_EVENT_BIOS_DONE,
    MAIN_CARD_EVENT_BIOS_ERROR,
    MAIN_CARD_EVENT_BIOS_TIMEOUT,
    MAIN_CARD_EVENT_BIOS_NEW_CARD,
    MAIN_CARD_EVENT_HARDWARE_DONE,
    MAIN_CARD_EVENT_HARDWARE_ERROR,
    MAIN_CARD_EVENT_HARDWARE_TIMEOUT,
    MAIN_CARD_EVENT_HARDWARE_NEW_CARD,
    MAIN_CARD_EVENT_COUNT
};

extern s32 g_main_card_bios_events[MAIN_CARD_EVENT_COUNT];
extern u8 g_main_card_bios_events_opened;

void main_card_init_events(void);

/* save image */
/* PlayStation memory-card save-file header block (256 bytes), as built by
 * card_save_build_file_header and its WORLD twin
 * world_card_build_save_file_header. */
typedef struct card_save_header {
    u8 magic0;           /* 0x00: 'S' */
    u8 magic1;           /* 0x01: 'C' */
    u8 icon_flags;       /* 0x02: 0x11, one icon frame */
    u8 block_count;      /* 0x03 */
    u8 title[0x40];      /* 0x04: Shift-JIS save-file title */
    u8 reserved[0x1C];   /* 0x44 */
    palette_t icon_clut; /* 0x60 */
    u8 icon_image[0x80]; /* 0x80 */
} card_save_header_t;

/* The fixed leading bytes of card_save_header_t. */
enum {
    CARD_SAVE_HEADER_MAGIC_S = 'S',
    CARD_SAVE_HEADER_MAGIC_C = 'C',
    CARD_SAVE_HEADER_ICON_ONE_FRAME = 0x11,
    CARD_SAVE_HEADER_BLOCK_COUNT_ONE = 1,
};

/* What a save slot's line on the save screen shows: the first 0x18 bytes of a
 * save's data block. g_main_card_save_slot_metadata keeps one per slot. */
typedef struct card_save_summary {
    u8 slot;                 /* 0x00; save slot, 0xff while the buffer is incomplete */
    u8 name[UNIT_NAME_SIZE]; /* 0x01; save-description leader name, 0xfe-terminated */
    u8 name_terminator;      /* 0x11 */
    u8 job_id;               /* 0x12 */
    u8 level;                /* 0x13 */
    calendar_date_t date;    /* 0x14; month/day from script variables 0x2e/0x2f */
    u8 location;             /* 0x16; script variable 0x31, location text index */
    u8 format_version;       /* 0x17; CARD writes 4 and WORLD load requires 4 */
} card_save_summary_t;
typedef char card_save_summary_size_must_be_24[(sizeof(card_save_summary_t) == 24) ? 1 : -1];

/* The 0x1e00-byte memory-card save image, parity-covered per 128-byte block.
 * WORLD builds and loads it in g_world_load_work_buffer; CARD fills the same
 * layout from the SCUS globals in card_save_build_buffer through
 * g_card_save_buffer_pointer. */
typedef struct card_save_buffer {
    card_save_header_t header;   /* 0x000 */
    card_save_summary_t summary; /* 0x100 */
    u8 parity_bits[8];           /* 0x118; one parity bit per 128-byte block */
    s32 elapsed_seconds;         /* 0x120 */
    /* 0x124-0x1dbe: filled by world_card_build_save_image and
     * card_save_build_buffer from the SCUS globals named in their bcopy calls. */
    u8 treasure_acquisition_date_bits[0x35];     /* 0x124; 47 packed 9-bit dates and 1 spare bit */
    u8 land_discovery_date_bits[0x12];           /* 0x159; packed 9-bit discovery dates */
    u8 proposition_last_attempt_date_bits[0x6c]; /* 0x16b; packed 9-bit dates */
    u8 proposition_states[0x60];                 /* 0x1d7; low 6 bits are a remaining-day counter */
    u8 _unused_0237[1];                          /* 0x237; serialized and parity-covered */
    u8 saved_records[5][0x38];                   /* 0x238; wldcore_saved_record_t: one sound-novel reading state each */
    u32 saved_data_bits[40];                     /* 0x350 */
    u32 secondary_saved_data_bits[2];            /* 0x3f0 */
    u8 brave_story_character_ages[0x40];         /* 0x3f8 */
    u8 proposition_count;                        /* 0x438 */
    u8 active_propositions[8][9];                /* 0x439 */
    u8 _unused_0481[3];                          /* 0x481; serialized and parity-covered */
    u8 party_records[PARTY_ROSTER_SLOT_COUNT][0xe0]; /* 0x484; party_unit_t records truncated to 0xe0 bytes */
    u8 item_quantities[ITEM_ID_COUNT];               /* 0x1604; g_main_item_quantities */
    u8 poached_item_quantities[ITEM_ID_COUNT];       /* 0x1704; g_main_item_poached_quantities */
    /* 0x1804; first 0x40 bytes are Move-Find-Item flags, second half unknown. */
    u8 item_location_flags[0x80];
    s32 script_variables[0x100]; /* 0x1884 */
    game_options_fields_t
        options;                /* 0x1c84; copied to g_main_game_options by world_card_load_globals_from_save_image */
    u8 _unknown_1c88;           /* 0x1c88 */
    s8 item_type_order_0[0xc];  /* 0x1c89; signed, -1-terminated */
    u8 _unused_1c95[1];         /* 0x1c95; serialized and parity-covered */
    s8 item_type_order_1[8];    /* 0x1c96; signed, -1-terminated */
    u8 _unused_1c9e[1];         /* 0x1c9e; serialized and parity-covered */
    s8 item_type_order_2[7];    /* 0x1c9f; signed, -1-terminated */
    s8 item_type_order_3[5];    /* 0x1ca6; signed, -1-terminated */
    s8 item_type_order_4[5];    /* 0x1cab; signed, -1-terminated */
    s8 item_type_order_5[7];    /* 0x1cb0; signed, -1-terminated */
    s8 item_type_order_6[5];    /* 0x1cb7; signed, -1-terminated */
    u8 weapon_page_order[0x8a]; /* 0x1cbc */
    u8 helmet_page_order[0x1d]; /* 0x1d46 */
    u8 armor_page_order[0x25];  /* 0x1d63 */
    u8 accessory_page_order[0x21]; /* 0x1d88 */
    u8 item_page_order[0x15];      /* 0x1da9 */
    u8 _unused_1dbe[0x42];         /* 0x1dbe; parity-covered tail */
} card_save_buffer_t;

typedef char card_save_buffer_size_must_be_7680[(sizeof(card_save_buffer_t) == 7680) ? 1 : -1];

/* One selectable save-file icon: 16-color CLUT plus 16x16 4bpp image. */
typedef struct card_save_icon {
    palette_t clut; /* 0x00 */
    u8 image[0x80]; /* 0x20 */
} card_save_icon_t;

/* save slots */
/* The scanner initializes 15 file states, then allocates available card
 * blocks to usable new-save entries. State 0 means a readable save header. */
typedef enum card_save_slot_state {
    CARD_SAVE_SLOT_UNSCANNED = -2,
    CARD_SAVE_SLOT_UNAVAILABLE = -1,
    CARD_SAVE_SLOT_OCCUPIED = 0,
    CARD_SAVE_SLOT_AVAILABLE = 1
} card_save_slot_state_e;

enum { CARD_SAVE_SLOT_COUNT = 15 };

/* g_main_card_save_last_used_slot when no slot was loaded or written. */
enum {
    CARD_SAVE_LAST_USED_SLOT_NONE = 0xff,
};

/* Hours, minutes and seconds shown for one memory-card save slot. */
typedef struct card_save_play_time {
    u8 hours;
    u8 minutes;
    u8 seconds;
} card_save_play_time_t;
typedef char card_save_play_time_size_must_be_3[(sizeof(card_save_play_time_t) == 3) ? 1 : -1];

extern u8 g_main_card_save_last_used_slot; /* the save slot last loaded or written */
extern u8 g_main_card_save_selected_slot;
extern s8 g_main_card_save_slot_file_states[]; /* card_save_slot_state_e */
extern card_save_summary_t g_main_card_save_slot_metadata[CARD_SAVE_SLOT_COUNT];
extern card_save_play_time_t g_main_card_save_slot_playtimes[CARD_SAVE_SLOT_COUNT];

#endif

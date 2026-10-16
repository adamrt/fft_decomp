#ifndef MAIN_H
#define MAIN_H

/* SCUS_942.21: resident game services and data tables. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/libcd.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* ability */
typedef struct main_ability_human_skillset {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} main_ability_human_skillset_t;

typedef struct main_ability_monster_skillset {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} main_ability_monster_skillset_t;

/* One secondary-data byte per ability from ABILITY_ID_MATH_FIRST on (math,
 * reaction, support, movement). */
extern u8 g_main_math_rsm_ability_data[ABILITY_ID_COUNT - ABILITY_ID_MATH_FIRST];

/* The item_type_e each Throw ability throws, indexed by ability id - ABILITY_ID_THROW_FIRST. */
extern u8 g_main_throw_ability_item_types[];
extern main_ability_jump_record_t g_main_jump_ability_data[12];
extern main_ability_charge_record_t g_main_charge_ability_data[8];

/* Indexed by reaction ability id - ABILITY_ID_REACTION_FIRST. */
extern const u8 g_main_reaction_behavior_flags[ABILITY_ID_SUPPORT_FIRST - ABILITY_ID_REACTION_FIRST];
extern u8 g_main_calculator_primes[CALCULATOR_PRIME_COUNT];

/* card */
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

/* PlayStation memory-card save-file header block (256 bytes), as built by
 * card_save_build_file_header (0x801c2ea8) and its WORLD twin
 * world_card_build_save_file_header (0x801322e4). */
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
    u8 slot;              /* 0x00; save slot, 0xff while the buffer is incomplete */
    u8 name[0x10];        /* 0x01; save-description leader name, 0xfe-terminated */
    u8 name_terminator;   /* 0x11 */
    u8 job_id;            /* 0x12 */
    u8 level;             /* 0x13 */
    calendar_date_t date; /* 0x14; month/day from script variables 0x2e/0x2f */
    u8 location;          /* 0x16; script variable 0x31, location text index */
    u8 format_version;    /* 0x17; CARD writes 4 and WORLD load requires 4 */
} card_save_summary_t;
typedef char card_save_summary_size_must_be_24[(sizeof(card_save_summary_t) == 24) ? 1 : -1];

/* The 0x1e00-byte memory-card save image, parity-covered per 128-byte block.
 * WORLD holds it in g_world_load_work_buffer (0x801cd1ec) and its serializer
 * and loader establish the fields below; CARD fills the same layout from the
 * SCUS globals in card_save_build_buffer (0x801c01ac) through
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

/* Hours, minutes and seconds shown for one memory-card save slot. */
typedef struct card_save_play_time {
    u8 hours;
    u8 minutes;
    u8 seconds;
} card_save_play_time_t;
typedef char card_save_play_time_size_must_be_3[(sizeof(card_save_play_time_t) == 3) ? 1 : -1];

extern s32 g_main_card_bios_events[MAIN_CARD_EVENT_COUNT];
extern u8 g_main_card_bios_events_opened;
extern u8 g_main_card_save_last_used_slot; /* the save slot last loaded or written */
extern u8 g_main_card_save_selected_slot;
extern s8 g_main_card_save_slot_file_states[];
extern card_save_summary_t g_main_card_save_slot_metadata[CARD_SAVE_SLOT_COUNT];
extern card_save_play_time_t g_main_card_save_slot_playtimes[CARD_SAVE_SLOT_COUNT];

void main_card_init_events(void);

/* file */
/* Sector and byte length of a resource read through the resident file loader. */
typedef struct file_extent {
    s32 sector;
    s32 size;
} file_extent_t;
typedef char file_extent_size_must_be_8[(sizeof(file_extent_t) == 8) ? 1 : -1];

/* First sector of each disc file the code reads at a fixed sector, named as
 * the disc directory names the file. The other files are found through
 * sector tables in the data. */
typedef enum disc_sector {
    DISC_SECTOR_SCEAP_DAT = 0xc6,
    DISC_SECTOR_BATTLE_BIN = 0x3e8,
    DISC_SECTOR_EVENT_ATTACK_OUT = 0x990,
    DISC_SECTOR_EVENT_MAPTITLE_BIN = 0xdac, /* five sectors for each four maps */
    DISC_SECTOR_EVENT_FRAME_BIN = 0xe68,
    DISC_SECTOR_EVENT_TEST_EVT = 0xe7b,     /* EVENT_BLOCK_SECTORS per event_id_e */
    DISC_SECTOR_EVENT_EVTFACE_BIN = 0x164b, /* four sectors per portrait */
    DISC_SECTOR_EVENT_UNIT_BIN = 0x166b,
    DISC_SECTOR_EVENT_BONUS_BIN = 0x16c0, /* thirteen sectors per image */
    DISC_SECTOR_EVENT_ITEM_BIN = 0x1899,
    DISC_SECTOR_EVENT_WLDFACE_BIN = 0x18ba,
    DISC_SECTOR_EVENT_ATCHELP_LZW = 0x1a3a,
    DISC_SECTOR_EVENT_WORLD_LZW = 0x1bd8,
    DISC_SECTOR_EVENT_OPEN_LZW = 0x1c18,
    DISC_SECTOR_EVENT_EVTCHR_BIN = 0x1d4c, /* fifteen sectors per block */
    DISC_SECTOR_BATTLE_EFC_FNT_TIM = 0xdea8,
    DISC_SECTOR_BATTLE_WEP_SPR = 0xdefa,
    DISC_SECTOR_BATTLE_OTHER_SPR = 0xdf24,
    DISC_SECTOR_BATTLE_ENTD1_ENT = 0xebc1,
    DISC_SECTOR_BATTLE_ENTD2_ENT = 0xebe9,
    DISC_SECTOR_BATTLE_ENTD3_ENT = 0xec11,
    DISC_SECTOR_BATTLE_ENTD4_ENT = 0xec39,
    DISC_SECTOR_BATTLE_ZODIAC_BIN = 0xec61,
    DISC_SECTOR_MENU_FFTSAVE_DAT = 0x11a7f,
    DISC_SECTOR_WORLD_WLDTEX_TM2 = 0x11d28,
    DISC_SECTOR_WORLD_WLDPIC_BIN = 0x11dae,  /* by g_wldcore_picture_sector_offsets */
    DISC_SECTOR_WORLD_WLDMES_BIN = 0x11f59,  /* by g_wldcore_message_block_sector_offsets */
    DISC_SECTOR_WORLD_SNPLBIN_BIN = 0x128f9, /* by g_wldcore_sound_novel_script_sector_offsets */
    DISC_SECTOR_WORLD_SNPLMES_BIN = 0x1291d, /* by g_wldcore_sound_novel_text_sector_offsets */
    DISC_SECTOR_WORLD_WLDBK_BIN = 0x12995,   /* by g_wldcore_background_sector_offsets */
    DISC_SECTOR_WORLD_WLDCORE_BIN = 0x14849,
    DISC_SECTOR_WORLD_WORLD_BIN = 0x14925,
    DISC_SECTOR_SOUND_ENV_SED = 0x14c08,
    DISC_SECTOR_SOUND_SYSTEM_SED = 0x14c0a,
    DISC_SECTOR_SOUND_WAVESET_WD = 0x14c0f,
    DISC_SECTOR_OPEN_OPEN_BIN = 0x14ff0,
    DISC_SECTOR_OPEN_OPNTEX_BIN = 0x1505d,
    DISC_SECTOR_OPEN_OPNBK_BIN = 0x15243, /* the title backgrounds, by sector offset */
    DISC_SECTOR_OPEN_FFTST_STR = 0x153d6,
    DISC_SECTOR_OPEN_FFTOP_STR = 0x17b60,
    DISC_SECTOR_OPEN_FFTPRE_STR = 0x1e038,
    DISC_SECTOR_OPEN_FFTEND_STR = 0x21908,
    DISC_SECTOR_OPEN_FFTPR_STR = 0x25160,
    DISC_SECTOR_OPEN_FFTUNIT_STR = 0x26ec0,
    DISC_SECTOR_OPEN_ENDING_XA = 0x2f551,
} disc_sector_e;

/* Loading-display mode passed to the shared file-header builder. */
typedef enum main_file_loading_display_mode {
    MAIN_FILE_LOADING_DISPLAY_OFF = -1,
    MAIN_FILE_LOADING_DISPLAY_INIT = 0,
    MAIN_FILE_LOADING_DISPLAY_PRESERVE = 1,
} main_file_loading_display_mode_e;

/* One poll advances at most one phase; recoverable CD errors restart setup. */
typedef enum main_file_load_state {
    MAIN_FILE_LOAD_STATE_IDLE = 0,
    MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED = 1,
    MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE = 2,
    MAIN_FILE_LOAD_STATE_START_SEEK = 3,
    MAIN_FILE_LOAD_STATE_POLL_SEEK = 4,
    MAIN_FILE_LOAD_STATE_START_READ = 5,
    MAIN_FILE_LOAD_STATE_POLL_READ = 6,
    MAIN_FILE_LOAD_STATE_UPLOAD_VRAM = 7, /* WLDCORE image stream only */
} main_file_load_state_e;

/* Asynchronous CD read state advanced by main_file_poll_load. */
typedef struct main_file_load_descriptor {
    s32 _unknown_00;
    s32 state; /* MAIN_FILE_LOAD_STATE_* */
    s32 error_count;
    s32 wait_frames;
    s32 sector_count;
    s32 lba;
    s32 sector_index; /* 0x18; sector being read by wldcore_file_poll_vram_image_stream */
    CdlLOC position;
    u8* destination;
} main_file_load_descriptor_t;
typedef char main_file_load_descriptor_size_must_be_36[sizeof(main_file_load_descriptor_t) == 36 ? 1 : -1];

extern s32 g_main_battle_bin_sector;

/* input */
/* L1 + R1 + Select + Start restarts the game. */
enum {
    MAIN_INPUT_RESET_BUTTONS = PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START,
};

extern u8 g_main_input_repeat_initial_delay;
extern u8 g_main_input_repeat_period;
extern u8 g_main_input_secondary_repeat_period;
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

/* item */

extern u8 g_main_weapon_page_order[0x8a];
extern u8 g_main_accessory_page_order[0x21];
extern u8 g_main_armor_page_order[0x25];
extern u8 g_main_helmet_page_order[0x1d];

/* job */

void job_calculate_and_store_unlocks(party_unit_t* party, u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]);
s32 job_find_first_for_skillset(s32 skillset);
s32 job_get_generic_index(s32 job_id);
s32 job_get_skillset(s32 job_id);
s32 job_is_special_monster(s32 job_id);

/* save */
/* Nine-byte active-proposition records at 0x80057eec. Daily update 0x80074b30
 * advances elapsed_days and sets completion bit 0x04. The assignment at
 * 0x80074a34 stores the current world-map location at +0x04; 0x80074c40
 * filters completed records by that byte. */
typedef struct wldcore_proposition_progress {
    u8 flags;          /* 0x00; wldcore_proposition_progress_flag_e */
    u8 proposition_id; /* 0x01; text 0xd000 | id names it */
    u8 elapsed_days;
    u8 assigned_days;
    u8 location;
    u8 participant_count;      /* 0x05; speaker selection at 0x800795c4 trusts this count */
    u8 participant_indices[3]; /* 0x06; 0x80074a8c passes each index to party lookup before 0x80074ab4 stores it */
} wldcore_proposition_progress_t;
typedef char wldcore_proposition_progress_size_must_be_9[sizeof(wldcore_proposition_progress_t) == 9 ? 1 : -1];

typedef struct wldcore_saved_record {
    /* 0x00: loaded sound-novel script, including its section-offset table. */
    struct wldcore_sound_novel_script_file* buffer;
    u32* script_words; /* 0x04 */
    u32 instruction;   /* 0x08 */
    /* 0x0c-0x37 mirror the live sound-novel state at 0x800d484c. */
    u16 state_flags;          /* 0x0c; wldcore_opcode_state_flag_e */
    s16 section;              /* 0x0e */
    s16 word_index;           /* 0x10; index of the 32-bit instruction in the section; cleared by
                               * wldcore_sound_novel_restore_saved_state
                               */
    s16 counter_delta;        /* 0x12 */
    s16 text_id;              /* 0x14 */
    s16 brightness;           /* 0x16 */
    s16 _unknown_18;          /* 0x18 */
    s16 background_set;       /* 0x1a */
    s16 picture_render_index; /* 0x1c */
    s16 picture_id;           /* 0x1e */
    s16 text_history_0;       /* 0x20; four-entry text history, newest last */
    s16 text_history_1;       /* 0x22 */
    s16 text_history_2;       /* 0x24 */
    s16 text_history_3;       /* 0x26 */
    s16 value_display_mode;   /* 0x28 */
    s16 music_selection;      /* 0x2a */
    s16 sound_mode;           /* 0x2c */
    s16 picture_x;            /* 0x2e */
    s16 picture_y;            /* 0x30 */
    s16 sound_id;             /* 0x32 */
    s32 counter;              /* 0x34 */
} wldcore_saved_record_t;
typedef char wldcore_saved_record_size_must_be_56[sizeof(wldcore_saved_record_t) == 56 ? 1 : -1];

/* Saved-data records at 0x80059594. The exact caller at 0x8008aba8 establishes
 * the 0x38 stride; 0x8008abe4 selectively resets these fields. Field meanings
 * beyond those stores remain provisional. */
struct wldcore_sound_novel_script_file;

extern u8 D_800473A7;

extern u8 D_80057b1c;

extern u8 g_main_save_proposition_count;
extern s32 g_main_saved_animation_speed;
extern s32 g_main_saved_weather_variable;

/* RAM bitmaps indexed by 32-bit values; the save serializer copies only their packed data
 * bytes and omits the padding at the end of each allocation. */
extern u32 g_main_proposition_last_attempt_date_bits[27];
extern u32 g_main_treasure_acquisition_date_bits[14];
extern u32 g_main_land_discovery_date_bits[5];
extern u8 g_main_proposition_states[96]; /* wldcore_proposition_state_e */
extern struct wldcore_sound_novel_script_file g_main_save_slot_buffer;
extern u32 g_main_saved_data_bits[40];
extern menu_cursor_state_t g_main_saved_list_cursor_state;
extern wldcore_proposition_progress_t g_main_active_propositions[8];
extern wldcore_saved_record_t g_main_saved_records[];
extern u32 g_main_tutorial_progress_masks[];
extern u32 g_main_secondary_saved_data_bits[2];

void main_save_init_data_tables(void);
void main_save_init_state_and_options(void);

/* script */
extern u16 g_main_scenario_event_finish_operations[];

/* The script variables: 32-bit values first (event_script_variable_id_e), then the packed flags. */
extern s32 g_main_script_variables[0x100];
extern u16 g_main_scenario_condition_offsets[];
extern s16 g_main_scenario_condition_data[];
extern event_file_block_t g_main_script_event_block[];

/* sound */
/* Scenario-music playback state at 0x8004d95c (SCUS_942.21). current_music
 * is the Suzuki music record of the playing scenario music:
 * main_sound_stop_current_music hands it straight to
 * suzuki_smd_stop_music, which takes a suzuki_music_t*. */
typedef struct main_sound_music_state {
    s32 current_slot;                 /* 0x00 0x8004d95c slot of the playing scenario music */
    suzuki_music_t* current_music;    /* 0x04 0x8004d960 */
    suzuki_smd_header_t* current_smd; /* 0x08 0x8004d964 SMD data of the playing music */
    s32 volume;                       /* 0x0c 0x8004d968 */
} main_sound_music_state_t;

/* Music slots 1..2 hold scenario music; slots 3..7 hold the five permanent
 * tunes. Both arrays store slot n at index n - 1. */
typedef struct main_sound_music_slots {
    s32 tune;                    /* 0x00: pending tune (id | 0x80) or playing tune id */
    s32 loading_slot;            /* 0x04: slot whose SMD load is in flight, or zero */
    suzuki_music_t* music[7];    /* 0x08: music records returned by suzuki_smd_create_music */
    suzuki_smd_header_t* smd[7]; /* 0x24: loaded SMD data */
} main_sound_music_slots_t;

/* The music state and slot table are one object: main_sound_open_generic_sfx
 * derives the slot pointers from the &state.current_smd base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct main_sound_music {
    main_sound_music_state_t state; /* 0x00 0x8004d95c */
    main_sound_music_slots_t slots; /* 0x10 0x8004d96c */
} main_sound_music_t;

/* Music ids: the index of a MUSIC_NN.SMD file in
 * g_main_sound_scenario_smd_files (MAIN_MUSIC_COUNT entries). Only the ids
 * the code passes by number are named, after the screen that plays them;
 * event scripts and scenario data choose the others. The music test offers
 * ids 1-96 (g_open_music_test_music_ids). */
typedef enum main_music {
    MAIN_MUSIC_NONE = 0,       /* the *_sound_play_music_tracks functions and the WLDCORE audio queue skip it */
    MAIN_MUSIC_ALTIMA = 19,    /* EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC */
    MAIN_MUSIC_MILESTONE = 20, /* wldcore_map_step_discovery_announcement, in the bar */
    MAIN_MUSIC_TUTORIAL = 26,  /* the world menu's tutorial screen */
    MAIN_MUSIC_WORLD_MAP = 27,
    MAIN_MUSIC_SHOP = 28,                         /* world_shop_run_screen: the item shop */
    MAIN_MUSIC_SOLDIER_OFFICE = 29,               /* world_shop_run_screen */
    MAIN_MUSIC_FUR_SHOP = 30,                     /* world_shop_run_screen */
    MAIN_MUSIC_FORMATION = 31,                    /* world_formation_run_screen */
    MAIN_MUSIC_BRAVE_STORY = 32,                  /* the Brave Story screens */
    MAIN_MUSIC_BAR = 33,                          /* the bar of a town */
    MAIN_MUSIC_NEW_GAME = 34,                     /* name and birthday entry of a new game */
    MAIN_MUSIC_PROPOSITION_REPORT = 35,           /* a finished proposition's report */
    MAIN_MUSIC_SAVE = 41,                         /* the memory card screens */
    MAIN_MUSIC_DEPLOYMENT = 42,                   /* the ATTACK unit placement screen */
    MAIN_MUSIC_CONGRATULATIONS = 43,              /* require_text_show_battle_congratulations */
    MAIN_MUSIC_CONGRATULATIONS_FINAL_BATTLE = 44, /* the same screen after event 0x145 */
    MAIN_MUSIC_GAME_OVER = 50,                    /* etc_graphic_show_game_over */
    MAIN_MUSIC_COUNT = 100,
} main_music_e;

/* Tunes of main_sound_play_tune: short pieces that fade the scenario music
 * out, play once and fade it back in (main_sound_update_tunes). Tune t is
 * MUSIC_(44 + t).SMD, kept loaded in music slot 2 + t. Tune 4 has no caller
 * in the code; EVENT_OPCODE_PLAY_TUNE passes its operand. */
typedef enum main_sound_tune {
    MAIN_SOUND_TUNE_NONE = 0,
    MAIN_SOUND_TUNE_LEVEL_UP = 1,
    MAIN_SOUND_TUNE_ITEM_FOUND = 2, /* an item found by moving, a poach, a trained monster */
    MAIN_SOUND_TUNE_JOB_LEVEL_UP = 3,
    MAIN_SOUND_TUNE_JOB_CHANGE = 5,
} main_sound_tune_e;

/* Packed tune-slot id and pending flag. */
enum {
    MAIN_SOUND_TUNE_ID_MASK = 0x3f,
    MAIN_SOUND_TUNE_PENDING = 0x80,
};

/* Sound resources. A sound id is SUZUKI_SOUND_ID(resource, sound): the id of a
 * loaded suzuki_sound_resource_t in the high 16 bits and the index of a
 * sound in it in the low. Each battle effect file brings a resource of its
 * own (battle_effect_load_sound_section). */
typedef enum main_sound_resource_id {
    MAIN_SOUND_RESOURCE_SYSTEM = 0,        /* SOUND/SYSTEM.SED: main_sfx_e */
    MAIN_SOUND_RESOURCE_ENV = 1,           /* SOUND/ENV.SED: main_env_sfx_e */
    MAIN_SOUND_RESOURCE_SOUND_NOVEL = 225, /* g_wldcore_sound_novel_sound_resource, 81 sounds */
} main_sound_resource_id_e;

/* Sound-effect ids passed to main_sound_play_sfx and the module queues that
 * feed it (wldcore_sound_play_effect, world_sound_dispatch_effect and the
 * g_*_sound_*effect_id globals); every path reaches suzuki_sound_start_sfx with
 * MAIN_SOUND_RESOURCE_SYSTEM in the high 16 bits, so this is one id space.
 * SYSTEM.SED holds MAIN_SFX_COUNT sounds; the ones named here are named
 * from their users in the code, the others are played only by event and
 * animation script data. The event frames store -1 in the pending globals
 * for "no request"; the dispatchers turn a negative id into MAIN_SFX_NONE. */
typedef enum main_sfx {
    /* Nothing queued. SYSTEM.SED entry 0 has no streams, so playing it stops
     * the channel pair main_sound_play_sfx uses;
     * wldcore_menu_step_map_path_level plays it when the path animation ends. */
    MAIN_SFX_NONE = 0,
    MAIN_SFX_CONFIRM = 1,
    MAIN_SFX_CANCEL = 2,
    MAIN_SFX_CURSOR_MOVE = 3,
    MAIN_SFX_GIL_TICKER = 4, /* looping count-up of the REQUIRE gil reward */
    MAIN_SFX_INVALID = 5,
    MAIN_SFX_PAGE_SWITCH = 6, /* L1/R1 unit and page switches */
    MAIN_SFX_UNEQUIP = 7,     /* remove equipment/abilities */
    MAIN_SFX_RED_ORBS = 8,    /* battle_effect_red_orbs_secondary_handler starts */
    MAIN_SFX_DISMISS = 9,     /* dismiss unit */
    MAIN_SFX_EQUIP = 10,      /* equip, set ability, best fit */
    MAIN_SFX_CAMERA_ROTATE_LEFT = 11,
    MAIN_SFX_CAMERA_ROTATE_RIGHT = 12,
    MAIN_SFX_CAMERA_ROTATE_STOP = 13, /* the rotation reached its target */
    /* Paired zoom sounds: battle_camera_toggle_zoom and battle_camera_zoom_map
     * play 0x11 with zoom action 4 and 0xe with action 2; the in/out reading
     * follows the zoom-target comment in battle_camera_toggle_zoom. The
     * ATTACK deploy screen also plays 0x11 when a placed unit is moved. */
    MAIN_SFX_CAMERA_ZOOM_IN = 14,
    /* L1/R1 on the world map (wldcore_menu_step_map_cursor_level): 0x10 when
     * the toggle sets the zoomed-out bit of g_wldcore_map_zoom_motion, 0xf
     * when it clears it. */
    MAIN_SFX_WORLD_MAP_ZOOM_IN = 15,
    MAIN_SFX_WORLD_MAP_ZOOM_OUT = 16,
    MAIN_SFX_CAMERA_ZOOM_OUT = 17,
    MAIN_SFX_WINDOW_OPEN = 18, /* help, message or text window opens */
    /* The weapon sounds of battle_sound_play_weapon_sfx, by the weapon's
     * item_type_e: g_sound_weapon_swing_sfx_ids (the swing, played
     * before the hit is known), g_sound_weapon_hit_sfx_ids and
     * g_sound_shield_deflect_sfx_ids. */
    MAIN_SFX_HIT_BLADE = 19,    /* knife, ninja blade, sword, knight sword, katana */
    MAIN_SFX_HIT_HEAVY = 21,    /* axe, flail; also the deflect of a fist, bag or cloth */
    MAIN_SFX_HIT_UNARMED = 24,  /* fist, bag, cloth */
    MAIN_SFX_SWING_LIGHT = 25,  /* fist, knife, ninja blade, instrument, book, bag, cloth */
    MAIN_SFX_SWING_MEDIUM = 26, /* sword, knight sword, katana, rod, staff */
    MAIN_SFX_SWING_HEAVY = 27,  /* axe, flail, polearm, pole */
    /* Started by the spell-charge and summon-charge secondary effects, which
     * stop them with suzuki_sound_stop_sfx_channels when they finish. */
    MAIN_SFX_SPELL_CHARGE = 31,
    MAIN_SFX_SUMMON_CHARGE = 33,
    MAIN_SFX_VENOM_TRAP = 34, /* battle_effect_venom_trap_secondary_handler starts */
    /* battle_sound_play_landing_sfx_by_surface_type: water surfaces play
     * 0x23, grassland, thicket and rug 0x28, wooden floor, tree and box 0x3a,
     * every other surface 0x29. A step that ends on another unit also plays
     * 0x28, and mounting a unit named by battle id plays 0x29. */
    MAIN_SFX_LAND_WATER = 35,
    MAIN_SFX_JUMP = 39, /* a jump rise starts; the fall that ends a mount ride starts */
    MAIN_SFX_LAND_SOFT = 40,
    MAIN_SFX_LAND_DEFAULT = 41,
    MAIN_SFX_TEXT_PAGE = 45,            /* dialogue/scroll window advances a page */
    MAIN_SFX_SHIELD_DEFLECT_LIGHT = 46, /* blades, rod, staff, polearm, pole */
    MAIN_SFX_SHIELD_DEFLECT_HEAVY = 47, /* axe, flail */
    /* An error message opens: a memory-card load, save or scan failure, or a
     * refused equip, dismissal, job change or ability.
     * battle_sound_play_weapon_sfx plays it for an attack stopped by an
     * accessory or class evade, a forced failure, a blade grasp or a catch,
     * and as the shield deflect of an instrument or book. */
    MAIN_SFX_ERROR_MESSAGE = 48,
    MAIN_SFX_CAMERA_TILT = 49, /* battle_camera_handle_tilt_request */
    MAIN_SFX_HIT_ROD = 56,     /* rod, staff */
    MAIN_SFX_LAND_WOOD = 58,
    /* Circle on a name-entry character key, or on an occupied tile of the
     * ATTACK deploy screen. */
    MAIN_SFX_SELECT = 60,
    MAIN_SFX_HIT_POLE = 62,         /* polearm, pole */
    MAIN_SFX_EVENT_REVIVE = 65,     /* the event script's revive status change (*_script_inflict_status_thread) */
    MAIN_SFX_WORLD_MAP_ARRIVE = 67, /* a route leg ends at a location with no event or encounter */
    /* Dead status added outside an event
     * (battle_unit_update_animation_for_status_changes), chosen by the unit's
     * male, female or monster flag; the male sound is 0x9a. */
    MAIN_SFX_DEATH_FEMALE = 69,
    MAIN_SFX_DEATH_MONSTER = 70,
    MAIN_SFX_SHIELD_DEFLECT_BULLET = 71, /* gun */
    MAIN_SFX_SHIELD_DEFLECT_ARROW = 73,  /* crossbow, bow */
    /* Proposition result (g_wldcore_proposition_report.result): 0 plays the
     * 0x4d/0x4e pair through suzuki_sound_play_2_sfx, 1 the 0x4f/0x50 pair,
     * 2 plays 0x51. */
    MAIN_SFX_PROPOSITION_SUCCESS_A = 77,
    MAIN_SFX_PROPOSITION_SUCCESS_B = 78,
    MAIN_SFX_PROPOSITION_PARTIAL_A = 79,
    MAIN_SFX_PROPOSITION_PARTIAL_B = 80,
    MAIN_SFX_PROPOSITION_FAILURE = 81,
    MAIN_SFX_SWING_BOW = 82,
    MAIN_SFX_SWING_CROSSBOW = 86,
    MAIN_SFX_SWING_GUN = 90,
    MAIN_SFX_HIT_GUN = 91,
    MAIN_SFX_TELEPORT_OUT = 106,
    MAIN_SFX_TELEPORT_IN = 107,
    /* Animation step 4 of the world event-transition indicator
     * (wldcore_menu_step_event_transition_level): 0x6c in mode 0, which a
     * random encounter pushes, 0x6d in the other spinning modes. */
    MAIN_SFX_EVENT_TRANSITION_MODE_0 = 108,
    MAIN_SFX_EVENT_TRANSITION_MODE_1 = 109,
    MAIN_SFX_STATUS_MENU_REQUEST
    = 113,                       /* Triangle queues the status menu on another unit's turn; again when it opens */
    MAIN_SFX_CRITICAL_HIT = 114, /* a weapon hit whose action is critical (battle_sound_play_weapon_sfx) */
    MAIN_SFX_TEXT_GLYPH = 115,   /* typewriter tick after a dialogue glyph */
    MAIN_SFX_HIT_ARROW = 116,    /* crossbow, bow */
    /* World map: 0x76 when a path animation starts, drawing or erasing; 0x77
     * when a hidden location is made visible. */
    MAIN_SFX_WORLD_MAP_PATH = 118,
    MAIN_SFX_WORLD_MAP_LOCATION_APPEAR = 119,
    /* A save or load finished, Start pressed in the opening's new-game
     * transition or to end deployment, a crystal or treasure picked up, or a
     * proposition report showing its reward picture. */
    MAIN_SFX_COMPLETE = 133,
    MAIN_SFX_SHOP_TRANSACTION = 151, /* shop buy/sell confirmed */
    MAIN_SFX_ABILITY_LEARNED = 152,  /* an ability bought in the learn list */
    MAIN_SFX_DEATH_MALE = 154,
    MAIN_SFX_HIT_BOOK = 156, /* instrument, book */
    MAIN_SFX_COUNT = 168,    /* the sound count in the SYSTEM.SED header */
} main_sfx_e;

/* Sounds of MAIN_SOUND_RESOURCE_ENV that the code plays by number. ENV.SED
 * holds 27 (0, 0x15, 0x16 and 0x17 are empty); event scripts choose the
 * others through EVENT_OPCODE_BG_SOUND. */
typedef enum main_env_sfx {
    MAIN_ENV_SFX_RAIN = 2,      /* battle_map_run_weather_command: rain without snow, and the thunder command */
    MAIN_ENV_SFX_THUNDER_A = 3, /* battle_map_draw_mesh_and_weather picks one of the two for a lightning flash */
    MAIN_ENV_SFX_THUNDER_B = 14,
} main_env_sfx_e;

/* status */
/* Values accepted by main_status_set_action_state. */
typedef enum main_unit_action_state {
    MAIN_UNIT_ACTION_STATE_NONE = 0,
    MAIN_UNIT_ACTION_STATE_CHARGING = 5,
    MAIN_UNIT_ACTION_STATE_JUMPING = 6,
    MAIN_UNIT_ACTION_STATE_DEFENDING = 7,
    MAIN_UNIT_ACTION_STATE_PERFORMING = 8,
    MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING = 255
} main_unit_action_state_e;

/* Packed highest-order action status: a one-based index, or zero for none. */
enum {
    MAIN_ACTION_STATUS_RESULT_INDEX_MASK = 0x7f,
    MAIN_ACTION_STATUS_RESULT_REMOVAL = 0x80,
    MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED = 0x100,
};

/* main_status_change_unit, 0x8005e6cc: OR, AND-not, or replacement of
 * inflicted_status, followed by merging status_sets.innate into status_sets.current. */
typedef enum main_status_change {
    MAIN_STATUS_ADD = 0,
    MAIN_STATUS_REMOVE = 1,
    MAIN_STATUS_REPLACE = 2
} main_status_change_e;

/* system */
/* Bits of g_main_system_flags, the 32-bit control flags of the WLDCORE world-map
 * loop: frame state, what the frame leaves out, requests left by WORLD and
 * OPENING, and the reason the loop ends. */
typedef enum main_system_flags {
    /* wldcore_entrypoint runs frames while set; wldcore_init_world_map_state sets it, a level clears it to leave. */
    MAIN_SYSTEM_FLAG_RUNNING = 0x00000001,
    /* Draw this frame with the 3D map; wldcore_gfx_draw_world_frame_with_map clears it after drawing. */
    MAIN_SYSTEM_FLAG_DRAW_MAP = 0x00000002,
    /* A level waits for a thread or a fade: wldcore_reset_game_if_special_keycode_is_pressed publishes no pad input. */
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
    /* window_dispatch_record_list_variant draws nothing and wldcore_map_pulse_dot_colors returns. */
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

/* g_main_system_game_flow_state: what follows when the BATTLE.BIN loop
 * closes a battle or event. */
typedef enum main_game_flow_state {
    MAIN_GAME_FLOW_BATTLE = 0, /* stay in battle_state_run_game_loop for the next event */
    MAIN_GAME_FLOW_WORLD = 1,  /* back to the world map */
    MAIN_GAME_FLOW_RESET = 2,  /* stop display and audio and restart from the opening */
    MAIN_GAME_FLOW_ENDING = 3, /* run the ending, then restart */
} main_game_flow_state_e;

/* Error codes passed to main_system_handle_error, whose report
 * hook is empty in retail. */
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

extern s32 D_800459D8;

extern s32 g_main_system_frontend_world_result;
extern u32 g_main_system_game_flow_state; /* main_game_flow_state_e */
extern u32 g_main_system_game_loop_stack_pointer;
extern u8 g_main_system_go_straight_to_battle;
extern int g_main_system_graphics_error;
extern s32 g_main_system_play_time_frames;
extern s32 g_main_system_play_time_hours;
extern s32 g_main_system_play_time_minutes;
extern s32 g_main_system_play_time_seconds;

/* main_system_flags_e bits; cleared by main_save_init_data_tables. */
extern u32 g_main_system_flags;
extern s32 g_main_system_session_frames;
extern s32 g_main_system_animation_speed;
extern s32 g_main_system_battle_frame_hsync_count;
extern s32 g_main_system_battle_frame_hsync_count_max;
extern s32 g_main_debug_display_enabled;
extern s32 g_main_replay_story_event_index;

/* 0x800449ec: empty (nop, jr ra). Every main loop and every blocking wait calls it once per frame. */
void main_system_frame_stub(void);

/* 0x800449f8: a return-only hook. Main and overlay callers supply two 32-bit arguments,
 * but neither their original meaning nor the hook's original name is proven. */
void main_system_report_stub(s32 category, s32 code);
void main_restore_game_loop_stack_pointer(u32* source);
void main_system_handle_error(int error_code);
void main_system_handle_battle_load_exception(s32 value);
void main_system_handle_malloc_exception(s32 allocator_id, s32 error_code);
void main_system_handle_pointer_exception(int error_code);
void main_system_reset_game(void);
void main_system_store_stack_pointer(u32* destination);
void main_system_handle_draw_sync_callback(void);
void main_system_handle_vsync_callback(void);

/* heap */
/* Game and music heaps allocate whole pages, tagged by these byte tables. */
enum {
    MAIN_HEAP_PAGE_SHIFT = 11,
    MAIN_HEAP_PAGE_SIZE = 1 << MAIN_HEAP_PAGE_SHIFT,
    MAIN_HEAP_GAME_PAGE_COUNT = 64,
    MAIN_HEAP_SMD_PAGE_COUNT = 16,
};

/* utilities */
s32 main_util_roll_pass_fail(s32 maximum, s32 threshold);
void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void main_util_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);

void main_util_copy_action_data(const action_command_t* source, action_command_t* destination);

/* unit */
/* Selects rows in the base-data (0x8005e90c) and variance (0x8005e93c)
 * tables; these are not job IDs or gender bit flags. */
typedef enum main_unit_generation_type {
    MAIN_UNIT_TYPE_MALE = 0,
    MAIN_UNIT_TYPE_FEMALE = 1,
    MAIN_UNIT_TYPE_RAMZA = 2,
    MAIN_UNIT_TYPE_MONSTER = 3,
    MAIN_UNIT_TYPE_COUNT = 4
} main_unit_generation_type_e;

/* One row of the generation tables at 0x8005e90c and 0x8005e93c. The
 * records are selected by main_unit_generation_type_e, not by job id. */
typedef struct main_unit_generation_base_data {
    u8 raw_stat_seeds[UNIT_RAW_STAT_COUNT];
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT];
} main_unit_generation_base_data_t;
typedef char main_unit_generation_base_data_size_must_be_12[(sizeof(main_unit_generation_base_data_t) == 12) ? 1 : -1];

extern unit_deployment_coordinates_t g_main_deployed_unit_map_coordinates[6];

/* zodiac */

extern u16 g_main_month_start_day_offsets[13];

/* gfx */
/* Resident unit-graphics queue, filled and consumed by the BATTLE loader. */
typedef struct main_gfx_unit_graphics_load_descriptor {
    u8 map_x;
    u8 map_y;
    u8 map_level;
    u8 _padding_03; /* aligns facing */
    u16 facing;     /* a quarter turn per unit */
    s16 spritesheet_id;
    u16 palette_id;
    u16 runtime_id;
    unit_stats_t* stats;
    u32 flags; /* nonzero: the unit starts undrawn */
} main_gfx_unit_graphics_load_descriptor_t;
typedef char main_gfx_unit_graphics_load_descriptor_size_must_be_20
    [(sizeof(main_gfx_unit_graphics_load_descriptor_t) == 20) ? 1 : -1];

/* The 0x144-byte queue occupies 0x80049c18..0x80049d5c. */
typedef struct main_gfx_unit_graphics_load_queue {
    s32 count;                                                                            /* 0x00 */
    main_gfx_unit_graphics_load_descriptor_t descriptors[BATTLE_UNIT_RUNTIME_SLOT_COUNT]; /* 0x04 */
} main_gfx_unit_graphics_load_queue_t;

typedef char
    main_gfx_unit_graphics_load_queue_size_must_be_324[(sizeof(main_gfx_unit_graphics_load_queue_t) == 324) ? 1 : -1];

/* EVENT/FRAME.BIN: 256x288 pixels at 4bpp and 22 palettes, sector rounded.
 * The loaders upload rows 32..287 and the two palette blocks. */
typedef struct main_gfx_frame_bin {
    u8 pixels[288][128];       /* 0x0000; two 4bpp pixels per byte */
    clut_t clut;               /* 0x9000; first 16 palettes */
    palette_t palette_tail[6]; /* 0x9200; remaining six palettes */
    u8 _padding_92c0[0x540];   /* 0x92c0; past the end of the file, to the sector boundary */
} main_gfx_frame_bin_t;
typedef char main_gfx_frame_bin_size_must_be_38912[(sizeof(main_gfx_frame_bin_t) == 38912) ? 1 : -1];

extern s32 g_main_font_print_enabled;
extern u8 g_main_common_menu_graphics_loaded;

void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue);

/* menu */
extern u8 g_main_menu_scroll_accel_delay;
extern u8 g_main_menu_scroll_slow_step;
extern u8 g_main_menu_scroll_fast_step;
extern u8 g_main_action_menu_types_by_skillset[0xe0];
extern game_options_t g_main_game_options;
extern game_options_t g_main_game_options_defaults;

/* terrain */
/* g_main_terrain_status_flags has one byte per map_surface_type_e; only
 * MAP_SURFACE_POISONED_MARSH has a bit. */
enum {
    MAIN_TERRAIN_STATUS_FLAG_POISON = 0x80, /* battle_status_prepare_terrain_poison poisons a unit standing there */
};

/* Rows of g_main_terrain_movement_cost_tables, chosen from the unit's movement
 * abilities by move_store_unit_movement_to_scratchpad; each has one
 * cost per map_surface_type_e. */
typedef enum main_terrain_cost_table {
    MAIN_TERRAIN_COST_TABLE_DEFAULT = 0,
    MAIN_TERRAIN_COST_TABLE_FLY_TELEPORT = 1, /* every surface costs 1 */
    MAIN_TERRAIN_COST_TABLE_FLOAT = 2,        /* every surface costs 1 */
    MAIN_TERRAIN_COST_TABLE_WATER = 3,        /* Move in Water and Walk on Water */
    MAIN_TERRAIN_COST_TABLE_UNDERWATER = 4,
    MAIN_TERRAIN_COST_TABLE_LAVA = 5,
} main_terrain_cost_table_e;

/* Costs in those rows that are not a number of Move points. */
enum {
    MAIN_TERRAIN_COST_WEATHER = 0,    /* replaced by move_get_weather_terrain_cost */
    MAIN_TERRAIN_COST_BLOCKED = 0xff, /* stays so under Any Ground */
};

extern const u8 g_main_terrain_movement_cost_tables[][64];
extern const u8 g_main_terrain_status_flags[64];
extern const u8 g_main_terrain_geomancy_abilities[];

/* Two bytes per map_surface_type_e; all 64 pairs are 0x19, 0x19. */
extern u8 g_main_terrain_tile_info_pairs[64][2];

#endif

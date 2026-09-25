#ifndef FFT_MAIN_H
#define FFT_MAIN_H

/* SCUS_942.21: the resident executable, its data tables and the Suzuki sound driver. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/cd.h"
#include "psx/spu.h"

struct battle_deployed_coords;

/* ability */
/* Jump ability record, 12 of them at g_main_jump_ability_data: horizontal
 * range and vertical reach. */
typedef struct battle_loader_jump_record {
    u8 range;    /* 0x00 */
    u8 vertical; /* 0x01 */
} battle_loader_jump_record_t;
typedef char battle_loader_jump_record_size_must_be_2[(sizeof(battle_loader_jump_record_t) == 2) ? 1 : -1];

typedef struct main_ability_human_skillset {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} main_ability_human_skillset_t;

typedef struct main_ability_monster_skillset {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} main_ability_monster_skillset_t;

extern u8 g_main_ability_aoe_data[];
extern u8 g_main_ability_ct_data[];
extern ability_secondary_data_t g_main_ability_range_data[];
extern u8 g_main_jump_charge_ability_data_by_ability_id[];
extern u8 g_main_throw_ability_item_types_by_ability_id[];

/* Pre-biased so the raw ability id indexes the SCUS secondary-data byte; see
 * main_ability_calculate_pointers_and_type. */
extern u8 g_main_math_rsm_ability_data_by_ability_id[];

/* Provisional: 12 item types indexed by throw ability id - 0x17e. */
extern u8 g_main_throw_ability_item_types[];
extern battle_loader_jump_record_t g_main_jump_ability_data[12];
extern ability_data_t g_main_ability_data[];

/* Linker alias biased so a reaction ability ID can index the 32-byte table at
 * 0x8005ebd0 directly; only IDs 0x1a6..0x1c5 are valid. */
extern const u8 g_main_reaction_behavior_flags_by_ability_id[];
extern main_ability_human_skillset_t g_main_ability_human_skillset_abilities[];
extern main_ability_monster_skillset_t g_main_ability_monster_skillset_abilities[];
extern u16 g_main_ability_temp_list[SKILLSET_ABILITY_LIST_COUNT];
extern u32 g_main_secondary_saved_data_bits[2];
s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index);
u16* main_ability_store_skillset_abilities(s32 skillset, s32 flags);
s32 main_ability_calculate_pointers_and_type(s32 id, u8** out_ability_data, u8** out_secondary_data);
u32 main_ability_get_known(battle_stats_t* unit, u32 skillset_id);

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

/* The software- and hardware-card pollers return the first event observed in
 * the IOE, ERROR, TIMOUT, NEW order installed by main_card_init_events. */
typedef enum card_io_result {
    CARD_IO_RESULT_PENDING = -1,
    CARD_IO_RESULT_COMPLETE = 0,
    CARD_IO_RESULT_ERROR = 1,
    CARD_IO_RESULT_TIMEOUT = 2,
    CARD_IO_RESULT_NEW_CARD = 3,
} card_io_result_e;

enum { CARD_SAVE_SLOT_COUNT = 15 };

/* PlayStation memory-card save-file header block (256 bytes), as built by
 * card_build_save_file_header (0x801c2ea8) and its WORLD twin
 * world_card_build_save_file_header (0x801322e4). */
typedef struct card_save_header {
    u8 magic0;           /* 0x00: 'S' */
    u8 magic1;           /* 0x01: 'C' */
    u8 icon_flags;       /* 0x02: 0x11, one icon frame */
    u8 block_count;      /* 0x03 */
    u8 title[0x40];      /* 0x04: Shift-JIS save-file title */
    u8 reserved[0x1C];   /* 0x44 */
    u8 icon_clut[0x20];  /* 0x60 */
    u8 icon_image[0x80]; /* 0x80 */
} card_save_header_t;

extern s32 g_main_card_bios_events[MAIN_CARD_EVENT_COUNT];
extern u8 g_main_card_bios_events_opened;
extern u8 g_card_save_last_written_slot;
extern u8 g_card_save_selected_slot;
extern s8 g_card_save_slot_file_states[];
extern u8 g_card_save_slot_metadata[][0x18];
extern u8 g_card_save_slot_playtimes[][3];
void init_card_earlysafe(s32 val);
s32 card_create_new(s32 port);
s32 card_info(s32 port);
s32 card_load(s32 port);
s32 card_status(s32 slot);
void main_card_init_events(void);

/* file */
/* One poll advances at most one phase; recoverable CD errors restart setup. */
typedef enum main_file_load_state {
    MAIN_FILE_LOAD_STATE_IDLE = 0,
    MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED = 1,
    MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE = 2,
    MAIN_FILE_LOAD_STATE_START_SEEK = 3,
    MAIN_FILE_LOAD_STATE_POLL_SEEK = 4,
    MAIN_FILE_LOAD_STATE_START_READ = 5,
    MAIN_FILE_LOAD_STATE_POLL_READ = 6,
} main_file_load_state_e;

/* Asynchronous CD read state advanced by main_file_poll_load. */
typedef struct main_file_load_descriptor {
    s32 unknown_00;
    s32 state; /* MAIN_FILE_LOAD_STATE_* */
    s32 error_count;
    s32 wait_frames;
    s32 sector_count;
    s32 lba;
    s32 sector_index; /* 0x18; sector being read by wldcore_file_poll_vram_image_stream */
    u8 position[4];
    u8* destination;
} main_file_load_descriptor_t;
typedef char main_file_load_state_size_must_be_36[sizeof(main_file_load_descriptor_t) == 36 ? 1 : -1];

extern main_file_load_descriptor_t g_main_file_cd_state;
extern u32 g_main_file_still_loading;

/* File-header wrapper at 0x80011bd0 forwards build's result. */
s32 main_file_build_header_nnl(main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);
s32 main_file_call_build_header(s32 sector, s32 size, void* destination);
int main_file_is_still_loading(void);
void main_file_reset_cd_subsystems(void);
void main_file_handle_cd_read_callback(void);
void main_file_handle_cd_ready_callback(void);
void main_file_reset_cdrom_cpu_ram(void);

int main_file_build_header(
    main_file_load_descriptor_t* state, int lba, int sector_count, void* destination, int loading_display_mode);

void* main_file_get_bin_as_tim(int sector, int size);
void* main_file_load_checked_to_address(int sector, int size, void* destination);
void* main_file_get_smd(int sector, int size);
void* main_file_get_tim(s32 sector, s32 size);
void* main_file_load_to_address(int sector, u32 size, void* destination);
void main_file_load_data_from_disc(int sector, int sectors, void* destination, int suppress_loading_display);
void main_file_poll_load(main_file_load_descriptor_t* state);
void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state);

/* input */
extern u32 g_main_input_raw_buttons;
extern u8 g_main_input_repeat_initial_delay;
extern u8 g_main_input_repeat_period;
extern u8 g_main_input_secondary_repeat_period;

/* item */
extern u8 g_main_item_quantities[ITEM_ID_COUNT];
extern item_attribute_t g_main_item_attributes[];
extern u8 g_main_item_page_order[0x15];

/* The first 0x40 bytes are Move-Find-Item flags; the second half is not yet
 * identified. Both halves are initialized and serialized as one block. */
extern u8 g_main_item_location_flags[0x80];
extern u8 g_main_item_poached_quantities[ITEM_ID_COUNT];
extern item_secondary_data_t g_main_item_secondary_data[];

/* &g_main_item_primary_data[0].secondary_data_id viewed as a stride-12 byte
 * array. main_unit_set_equipment_stats reads the secondary id through its own
 * %lo(g_main_item_primary_data+4) address while the type_flags test uses a
 * record pointer; reading both through item_data_t folds them onto one base
 * register and misses the byte-exact match. */
extern u8 g_main_item_secondary_data_ids[];
extern item_type_order_tables_t g_main_item_type_order_tables;

/* Views of g_main_item_type_order_tables.order_1..6 for the WORLD save-image
 * copies (world_card_build_save_image, world_card_load_globals_from_save_image),
 * whose code the member spelling changes. */
extern s8 g_main_item_type_order_1[8];
extern u8 g_main_item_type_order_2[7];
extern u8 g_main_item_type_order_3[5];
extern u8 g_main_item_type_order_4[5];
extern u8 g_main_item_type_order_5[7];
extern u8 g_main_item_type_order_6[5];
extern weapon_data_t g_main_item_weapon_data[]; /* 0x80063ab8 */
extern u8 g_main_weapon_page_order[0x8a];
extern u8 g_main_item_ability_item_ids_by_ability_id[];
extern u8 g_main_item_ability_item_ids[14];
extern u8 g_main_item_poach_table[48][2];
extern accessory_data_t g_main_item_accessory_data[];
extern armor_data_t g_main_item_helm_armor_secondary_data[];
extern item_data_t g_main_item_primary_data[];
extern shield_data_t g_main_item_shield_data[];
void main_item_init_new_game_inventory(void);
void main_item_init_order_tables(void);
item_data_t* main_item_get_data_pointer(s32 item_id);
s32 main_item_get_total_equipment_quantity(u8 item_id, s32 include_battle);

/* job */
extern job_data_t g_job_data[];
extern const u16 g_job_level_jp_requirements[8];
extern u16 g_main_job_jp_requirements_by_level[];
extern const u8 g_job_unlock_requirements[UNIT_CAREER_JOB_COUNT - 1][UNIT_JOB_LEVEL_BYTE_COUNT];
s32 main_job_add_proposition_jp(s32 party_index, s32 jp);
u32 main_job_calculate_level(u16 job_jp);
u32 main_job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags);
job_data_t* main_job_get_data_pointer(s32 job_id);
s32 main_job_get_random_unlocked(const battle_stats_t* unit);
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs);

/* save */
/* Byte view of the four-byte world-script instruction word at 0x800d4848:
 * opcode byte followed by up to three operand bytes. */
typedef struct wldcore_opcode_instruction_bytes {
    u8 opcode;
    u8 operand_0;
    u8 operand_1;
    u8 operand_2;
} wldcore_opcode_instruction_bytes_t;
typedef char wldcore_instruction_bytes_size_must_be_4[sizeof(wldcore_opcode_instruction_bytes_t) == 4 ? 1 : -1];

/* Nine-byte active-proposition records at 0x80057eec. Daily update 0x80074b30
 * advances elapsed_days and sets completion bit 0x04. The assignment at
 * 0x80074a34 stores the current world-map location at +0x04; 0x80074c40
 * filters completed records by that byte. */
typedef struct wldcore_proposition_progress {
    u8 flags;
    u8 proposition_id; /* 0x01; text 0xd000 | id names it (scus_94221_datatables.txt 80057eec "Prop ID") */
    u8 elapsed_days;
    u8 assigned_days;
    u8 location;
    u8 participant_count;      /* 0x05; speaker selection at 0x800795c4 trusts this count */
    u8 participant_indices[3]; /* 0x06; 0x80074a8c passes each index to party lookup before 0x80074ab4 stores it */
} wldcore_proposition_progress_t;
typedef char wldcore_proposition_progress_size_must_be_9[sizeof(wldcore_proposition_progress_t) == 9 ? 1 : -1];

/* Saved-data records at 0x80059594. The exact caller at 0x8008aba8 establishes
 * the 0x38 stride; 0x8008abe4 selectively resets these fields. Field meanings
 * beyond those stores remain provisional. */
typedef struct wldcore_saved_record {
    /* 0x00: the slot's save-block buffer; MAIN's initializer at 0x80059594
     * stores g_main_save_slot_buffer (0x800d0bbc) here for all five slots. */
    u8* buffer;
    u32* script_words; /* 0x04 */
    union {
        u32 word;
        wldcore_opcode_instruction_bytes_t bytes;
    } instruction; /* 0x08 */
    /* 0x0c-0x37 mirror the live sound-novel state at 0x800d484c. */
    u16 state_flags; /* 0x0c */
    s16 section;     /* 0x0e */
    s16 word_index;  /* 0x10; instruction word index in the section; cleared by wldcore_sound_novel_restore_saved_state
                      */
    s16 counter_delta;        /* 0x12 */
    s16 text_id;              /* 0x14 */
    s16 brightness;           /* 0x16 */
    s16 field_18;             /* 0x18 */
    s16 background_set;       /* 0x1a */
    s16 picture_render_index; /* 0x1c */
    s16 picture_id;           /* 0x1e */
    s16 text_history_0;       /* 0x20; four-entry text history, newest last */
    s16 text_history_1;       /* 0x22 */
    s16 text_history_2;       /* 0x24 */
    s16 text_history_3;       /* 0x26 */
    u16 value_display_mode;   /* 0x28 */
    u16 music_selection;      /* 0x2a */
    u16 sound_mode;           /* 0x2c */
    s16 picture_x;            /* 0x2e */
    s16 picture_y;            /* 0x30 */
    s16 sound_id;             /* 0x32 */
    s32 counter;              /* 0x34 */
} wldcore_saved_record_t;
typedef char wldcore_saved_record_size_must_be_0x38[sizeof(wldcore_saved_record_t) == 0x38 ? 1 : -1];

/* Provisional: an adjacent word pair copied as one structure. The paired
 * type reproduces the target's two-load/two-store schedule. */
typedef struct wldcore_word_pair {
    s32 first;
    s32 second;
} wldcore_word_pair_t;

extern u8 g_main_save_proposition_count;
extern s32 g_main_saved_animation_speed;
extern s32 g_main_saved_weather_variable;

/* Word-indexed RAM bitmaps; the save serializer copies only their packed data
 * bytes and omits the padding at the end of each allocation. */
extern u32 g_main_proposition_last_attempt_date_bits[27];
extern u32 g_main_treasure_acquisition_date_bits[14];
extern u32 g_main_land_discovery_date_bits[5];
extern u8 g_main_proposition_states[96];
extern u32 g_main_save_day;
extern u32 g_main_save_month;
extern u8 g_main_save_slot_buffer[];
extern u32 g_main_saved_data_bits[40];
extern wldcore_word_pair_t g_main_saved_list_cursor_state;
extern wldcore_proposition_progress_t g_main_active_propositions[8];
extern wldcore_saved_record_t g_main_saved_records[];
void main_save_init_data_tables(void);
void main_save_init_state_and_options(void);
void save_unit_to_party(battle_stats_t* unit, u32 save_formation);

/* script */
extern u16 g_scenario_event_finish_operations[];
extern s32 g_main_script_variables[0x100];
extern u8* g_event_overlay_load_address;
extern u16 g_main_scenario_condition_offsets[];
extern s16 g_main_scenario_condition_data[];
extern event_file_block_t g_event_script_buffer[];

/* sound */
/* Suzuki sound driver (SCUS_942.21 0x800120f4-0x800186c4) records, tables and
 * globals. Offsets and widths are taken from the driver code named beside
 * each field; field_XX and _unknown_XX members are provisional. */
/* Linear ramp stepped once per tick by main_smd_step_ramp (0x80014f18):
 * value moves by step until count reaches zero, then snaps to target << 16.
 * The root-counter handler tests count with `lh` before stepping MUS
 * +0x88/+0x94/+0xa0/+0xac and the volume ramps of g_main_sound_spu_state. */
typedef struct suzuki_ramp {
    s32 value;  /* 0x00 */
    s32 step;   /* 0x04 */
    s16 count;  /* 0x08 */
    s16 target; /* 0x0a */
} suzuki_ramp_t;

/* Suzuki heap block header (0x10 bytes; payload follows). The $gp-relative
 * allocator (0x8001423c-0x8001442c) rounds requests to 16 bytes and chains
 * blocks in address order from g_main_sound_heap_blocks. */
typedef struct suzuki_heap_block {
    u16 flags;                      /* 0x00; 0x8000 list head, 0x2 in use */
    u16 _unknown_02;                /* 0x02; cleared on allocation */
    u32 _unknown_04;                /* 0x04; cleared on allocation */
    u8* end;                        /* 0x08; end of the payload */
    struct suzuki_heap_block* next; /* 0x0c */
} suzuki_heap_block_t;

/* WAVESET instrument entry (0x10 bytes). main_smd_set_instrument
 * (0x80016fb4) and main_smd_get_instrument_attr copy it into a channel or an
 * SpuVoiceAttr. */
typedef struct suzuki_instrument {
    u32 start;        /* 0x00; sample offset from the waveset's SPU address */
    u16 loop_offset;  /* 0x04; loop start relative to start */
    u16 fine_tune;    /* 0x06; copied to the channel fine_tune */
    u8 attack_rate;   /* 0x08 */
    u8 decay_rate;    /* 0x09 */
    u8 sustain_rate;  /* 0x0a */
    u8 release_rate;  /* 0x0b */
    u8 sustain_level; /* 0x0c */
    u8 attack_mode;   /* 0x0d */
    u8 sustain_mode;  /* 0x0e */
    u8 release_mode;  /* 0x0f */
} suzuki_instrument_t;

/* WAVESET.WD header as loaded. main_sound_load_waveset
 * (0x80017c8c) uploads wave_size bytes from wave_offset to SPU RAM and keeps
 * header_size bytes on the Suzuki heap, linked from g_main_sound_waveset_list
 * through next. Select Sound Font (0x80016e48) matches id. */
typedef struct suzuki_waveset {
    u8 _unknown_00[0x10];               /* 0x00; "dwdsP" magic, file size at 0x08 */
    u32 header_size;                    /* 0x10 */
    u32 wave_size;                      /* 0x14 */
    u32 wave_offset;                    /* 0x18 */
    u8 _unknown_1c[4];                  /* 0x1c */
    u16 id;                             /* 0x20 */
    u8 _unknown_22[6];                  /* 0x22 */
    u32 spu_address;                    /* 0x28; SpuMalloc result, freed by main_sound_free_waveset */
    struct suzuki_waveset* next;        /* 0x2c */
    suzuki_instrument_t instruments[1]; /* 0x30 */
} suzuki_waveset_t;

/* SMD/SED sequence header ("smds"), read by main_smd_transfer_music_data,
 * main_smd_init_channels, main_smd_init_channel_streams and
 * main_smd_get_music_filename. */
typedef struct suzuki_smd_header {
    u8 magic[4];            /* 0x00 */
    u8 _unknown_04[4];      /* 0x04 */
    u32 size;               /* 0x08 */
    u8 _unknown_0c[4];      /* 0x0c */
    u16 id;                 /* 0x10; copied to MUS id and channel sound_id */
    u8 field_12;            /* 0x12; copied to MUS field_14 */
    u8 tick_divisor;        /* 0x13 */
    u8 channel_count;       /* 0x14 */
    u8 field_15;            /* 0x15; copied to MUS field_17 */
    u16 waveset_id;         /* 0x16 */
    u16 field_18;           /* 0x18; copied to MUS field_1a */
    s8 reverb_mode;         /* 0x1a */
    u8 reverb_depth;        /* 0x1b */
    u8 reverb_delay;        /* 0x1c */
    u8 reverb_feedback;     /* 0x1d */
    u16 filename_offset;    /* 0x1e */
    u16 data_offset;        /* 0x20 */
    u16 channel_offsets[1]; /* 0x22; zero-terminated */
} suzuki_smd_header_t;

/* Suzuki sequencer modulator (0x20 bytes). Each channel has four, at
 * channel+0xe0. step is one of the g_main_smd_modulator_waveforms functions,
 * called by main_smd_update_modulators (0x8001749c), which adds the scaled
 * result to the channel output selected by target. counter_14 delays the
 * start; while counter_18 is below 0x100 the output is scaled by
 * counter_18 / 0x100 and counter_18 grows by counter_18_reset (fade-in). */
typedef struct suzuki_modulator {
    s32 (*step)(struct suzuki_modulator*); /* 0x00 */
    s32 value;                             /* 0x04 */
    s32 increment;                         /* 0x08 */
    s32 amplitude;                         /* 0x0c; from main_smd_modulator_calculate_step */
    u16 timer;                             /* 0x10 */
    u16 timer_reset;                       /* 0x12 */
    u16 counter_14;                        /* 0x14 */
    u16 counter_14_reset;                  /* 0x16 */
    s16 counter_18;                        /* 0x18 */
    u16 counter_18_reset;                  /* 0x1a */
    u8 target;                             /* 0x1c; 0 pitch, 1 volume, 2 balance */
    u8 waveform;                           /* 0x1d; index into g_main_smd_modulator_waveforms */
    s16 flags;                             /* 0x1e; bit 0: active; bits 2-3: step state */
} suzuki_modulator_t;
typedef char suzuki_modulator_size_must_be_0x20[(sizeof(suzuki_modulator_t) == 0x20) ? 1 : -1];

typedef s32 (*suzuki_modulator_step_t)(suzuki_modulator_t* modulator);

/* Repeat stack entry (0xc bytes) pushed by Repeat (0x98) and consumed by
 * Coda (0x99) and To Coda (0x9A). */
typedef struct suzuki_repeat {
    u8 count;       /* 0x00 */
    u8 _unknown_01; /* 0x01 */
    u8 octave_base; /* 0x02 */
    u8 coda_octave; /* 0x03 */
    u8* start;      /* 0x04 */
    u8* coda;       /* 0x08 */
} suzuki_repeat_t;

/* Suzuki sequencer channel record (0x160 bytes), the third argument of every
 * SMD opcode handler (note data, music, channel). Field names follow the
 * opcode that writes them; field_XX members are written by unnamed opcodes.
 *
 * active is the note flag word: 0x1 in use, 0x4 set from MUS status 0x2000,
 * 0x8 keeps the velocity, 0x20 muted, 0x100 fermata, 0x400 note end or rest,
 * 0x800 set and cleared by opcodes 0xB0/0xB1, 0x4000 stream set
 * (main_smd_init_channel_streams); main_smd_set_flags_all_channels ORs a mask
 * into every non-zero word.
 *
 * func_flags marks the SPU voice attributes main_sound_update_voice_attrs
 * pushes: 0x1 volume, 0x2 volume and volume mode, 0x4 pitch, 0x8 start and
 * loop addresses, 0x10 attack (time and mode), 0x20 decay, 0x40 sustain
 * (time and mode), 0x80 release (time and mode), 0x100 sustain level. 0x4,
 * 0x10 and 0x40 also request the pitch-LFO, noise and reverb voice-mask
 * updates. note_flags2 bit 0 queues a key-on, bit 1 a key-off, bit 8 a
 * volume/balance change and bit 9 a pitch change for main_smd_update_voices.
 * flags_06 bits 0, 3 and 4 mark the pitch-slide, volume and balance ramps
 * stepped by main_smd_update_channel_ramps.
 *
 * octave_base is written at halfword width (Octave, Raise/Lower Octave, End
 * Bar & Loop) but saved into the u8 loop_octave_base by Loop (0x91) and
 * opcode 0x8D, which read it with `lbu`: GCC narrows the u16 load itself.
 *
 * sound_id is the SMD/SED id for music and `resource id << 16 | n` for VFX
 * sounds; Play VFX SMD (0x80015c38) reads the resource id half with `lh`,
 * the SFX API compares the whole word. */
typedef struct suzuki_music_channel {
    u16 active;      /* 0x00 */
    u16 note_flags2; /* 0x02 */
    u16 func_flags;  /* 0x04 */
    u16 flags_06;    /* 0x06; bit 1 toggled by 0xD5, bit 2 by 0xD6, bit 3 ramp active */
    union {
        u32 raw;
        struct {
            u16 low;
            s16 high; /* VFX resource id */
        } halves;
    } sound_id;                /* 0x08 */
    u8 channel_number;         /* 0x0c; 0xff + channel index (SFX: the index); write-only */
    u8 priority;               /* 0x0d; SFX steal priority (channel id >> 8; stolen when <= 0x20); music: 0x10 */
    u8 _unknown_0e[2];         /* 0x0e */
    u32 start_tick;            /* 0x10; g_main_sound_tick_count when main_sound_start_sfx started the channel */
    u8* note_data_start;       /* 0x14 */
    u8* note_data;             /* 0x18; read position */
    u8* loop_note_data;        /* 0x1c; set by Loop and opcode 0x8D, jumped to by End Bar & Loop */
    u8* restart_note_data;     /* 0x20; set by main_smd_init_channel_streams */
    u32 field_24;              /* 0x24; cleared by the channel initialiser */
    u16 loop_count;            /* 0x28; End Bar & Loop */
    u8 field_2a;               /* 0x2a; cleared by the channel initialiser */
    u8 loop_octave_base;       /* 0x2b; octave_base saved with loop_note_data */
    u8 instrument;             /* 0x2c */
    u8 voice;                  /* 0x2d; SPU voice (opcode 0xAA); bit 0 allows the pitch LFO */
    u16 release_2e;            /* 0x2e; Release stores its byte here and in release_time */
    suzuki_waveset_t* waveset; /* 0x30; Select Sound Font */
    u32 voice_mask;            /* 0x34; SPU voice bit(s) owned by this channel */
    u32 field_38;              /* 0x38; initialised to 0xff9f */
    s16 spu_volume_left;       /* 0x3c; SpuSetVoiceVolume(Attr) left, from main_smd_update_voices */
    s16 spu_volume_right;      /* 0x3e */
    s16 spu_volume_mode_left;  /* 0x40; SpuSetVoiceVolumeAttr left mode; cleared by the channel initialiser */
    s16 spu_volume_mode_right; /* 0x42 */
    u8 _unknown_44[4];         /* 0x44 */
    u16 spu_pitch;             /* 0x48; SpuSetVoicePitch value, from main_smd_update_voices */
    u8 _unknown_4a[6];         /* 0x4a */
    u32 start_address;         /* 0x50; instrument sample SPU address */
    u32 loop_address;          /* 0x54; instrument loop SPU address */
    s32 attack_mode;           /* 0x58 */
    s32 sustain_mode;          /* 0x5c */
    s32 release_mode;          /* 0x60 */
    u16 attack_time;           /* 0x64 */
    u16 decay_time;            /* 0x66 */
    u16 sustain_time;          /* 0x68 */
    u16 release_time;          /* 0x6a */
    u16 sustain_level;         /* 0x6c */
    u8 _unknown_6e[6];         /* 0x6e */
    s16 rest_length;           /* 0x74; Rest or Fermata hold length, counted down per tick */
    u8 length_adjust;          /* 0x76; signed note-length adjust: opcode 0xAD adds to or clears it */
    u8 _unknown_77;            /* 0x77 */
    u16 note_ticks;            /* 0x78; ticks left in the note; 0 sets active bit 0x400 */
    u16 gate_time;             /* 0x7a; key-on time in sixteenths of the length (0xA9); initialised to 0xf */
    u8 previous_key;           /* 0x7c; previous note's key, the portamento start; cleared by the channel initialiser */
    u8 key;                    /* 0x7d; key number of the current note */
    u16 octave_base;           /* 0x7e; key number of the current octave, octave * 12 */
    s32 pitch_slide;           /* 0x80; accumulated by the 0xD4 ramp */
    s16 fine_tune;             /* 0x84; instrument fine_tune, added to key << 8 (1/256 semitone) */
    s16 pitch_offset;          /* 0x86; fine pitch; 0xD0/0xD1 scale their byte by 32, 0xD2 by 8 */
    s16 pitch_modulation;      /* 0x88; modulator target 0 output */
    s16 volume_modulation;     /* 0x8a; modulator target 1 output */
    s16 balance_modulation;    /* 0x8c; modulator target 2 output */
    u8 _unknown_8e[2];         /* 0x8e */
    u16 portamento_ticks;      /* 0x90; portamento length (opcode 0xD6), mirrored into flags_06 bit 2 */
    s16 balance;               /* 0x92; balance byte << 8 */
    s16 velocity;              /* 0x94; velocity << 8, 0x6000 by default */
    u8 _unknown_96[2];         /* 0x96 */
    s32 volume;                /* 0x98; volume byte << 24 */
    s32 pitch_slide_step;      /* 0x9c; opcode 0xD4 */
    s32 volume_step;           /* 0xa0; Fermata Ramp (0xE2) */
    s16 balance_step;          /* 0xa4; opcode 0xEA */
    u16 pitch_slide_count;     /* 0xa6 */
    u16 volume_step_count;     /* 0xa8 */
    u16 balance_step_count;    /* 0xaa */
    u16 repeat_depth;          /* 0xac; -1 when the repeat stack is empty */
    u16 modulator_index;       /* 0xae; modulator selected by opcode 0xF0 */
    suzuki_repeat_t repeats[4];       /* 0xb0 */
    suzuki_modulator_t modulators[4]; /* 0xe0 */
} suzuki_music_channel_t;
typedef char suzuki_music_channel_size_must_be_0x160[(sizeof(suzuki_music_channel_t) == 0x160) ? 1 : -1];

/* Suzuki MUS (music sequence) record: 0xb8 bytes followed by channel_count
 * channels, allocated by SuzukiPutPlaySMD. It is the second argument of every
 * SMD opcode handler. Queued records form a linked list through next, headed
 * by g_main_sound_active_music_list; snapshot chains saved copies of the
 * whole record (main_smd_save_snapshot) through their own snapshot fields.
 *
 * status bit 15 is set while the music plays (SuzukiGetMusicPlaying);
 * main_sound_calculate_reverb_voices tests that bit as a sign (`lh` and
 * `bgez`), and bit 0 there marks the reverb voices as excludable. Bit 4 marks
 * a saved snapshot, bit 1 the SFX record, 0x100 music paused by
 * main_smd_pause_music (cleared on resume), 0x4000 a pending full voice
 * update (main_sound_update_voice_attrs clears it) and 0x2000 sets channel
 * active bit 0x4 at initialisation.
 *
 * tempo is tempo byte << 16. It is a union because Scale Tempo (0x80016e14)
 * reads the tempo byte back with `lh 0x7e`, while Tempo, Accelerando and
 * opcode 0xA2 use the whole word. */
typedef struct suzuki_music {
    struct suzuki_music* next;     /* 0x00 */
    struct suzuki_music* snapshot; /* 0x04; freed by main_smd_free_snapshots */
    suzuki_smd_header_t* smd;      /* 0x08 */
    u8 _unknown_0c[4];             /* 0x0c */
    u16 status;                    /* 0x10 */
    u16 id;                        /* 0x12; SMD id */
    u8 field_14;                   /* 0x14; SMD field_12 */
    u8 tick_divisor;               /* 0x15 */
    u8 channel_count;              /* 0x16 */
    u8 field_17;                   /* 0x17; SMD field_15 */
    s16 waveset_id;                /* 0x18; sound font id */
    u16 field_1a;                  /* 0x1a; SMD field_18, 0x7f for SFX */
    u8 field_1c;                   /* 0x1c; set by opcode 0xA4, adjusted by 0xA5 */
    u8 channel_select;             /* 0x1d; opcode 0x8D acts when its byte matches */
    u16 noise_clock;               /* 0x1e; opcodes 0xB4/0xB5 */
    s32 tick_20;                   /* 0x20; incremented every tick */
    u32 tick_24;                   /* 0x24; ticks while the record owns voices */
    u32 ticks;                     /* 0x28; root-counter ticks played, in 1/256 units (tempo scale) */
    u32 tick_2c;                   /* 0x2c; restore the snapshot when tick_24 reaches it (0: never) */
    u16 min_loop_count;            /* 0x30; main_smd_get_min_loop_count */
    u16 bar;                       /* 0x32; 1-based bar counter */
    u16 beat;                      /* 0x34; beat within the bar */
    u16 beat_ticks_36;             /* 0x36 */
    u16 beats_per_bar;             /* 0x38 */
    u16 ticks_per_beat;            /* 0x3a */
    u16 beat_unit;                 /* 0x3c */
    u16 beats_remaining;           /* 0x3e */
    u8 _unknown_40[4];             /* 0x40 */
    s32 reverb_mode;               /* 0x44 */
    s16 reverb_depth;              /* 0x48; depth byte << 8 */
    u8 _unknown_4a[2];             /* 0x4a */
    s32 reverb_delay;              /* 0x4c */
    s32 reverb_feedback;           /* 0x50 */
    u16 stop_bar;                  /* 0x54; main_smd_set_stop_bar: the music stops when bar reaches it */
    u8 _unknown_56[2];             /* 0x56 */
    u32 channel_mask;              /* 0x58; bit n: channel n has note data and is still running */
    u32 mute_mask;                 /* 0x5c; muted channels */
    u32 key_on_mask;               /* 0x60 */
    u32 key_off_mask;              /* 0x64 */
    u32 lfo_voice_mask;            /* 0x68 */
    u32 noise_voice_mask;          /* 0x6c */
    u32 reverb_voice_mask;         /* 0x70 */
    s32 tempo_accumulator;         /* 0x74; one sequencer tick per 0x10000 of scaled_tempo; initialised to 0x10000 */
    s32 scaled_tempo;              /* 0x78; tempo * tempo_scale */
    union {
        s32 raw;
        struct {
            u16 low;
            s16 high; /* the tempo byte itself */
        } halves;
    } tempo;                            /* 0x7c */
    s32 tempo_step;                     /* 0x80; per-step tempo delta of opcode 0xA2 */
    u16 tempo_step_count;               /* 0x84 */
    u16 tempo_target;                   /* 0x86; tempo byte the 0xA2 ramp approaches */
    suzuki_ramp_t tempo_scale;          /* 0x88; value is scale byte << 24, its high halfword the multiplier */
    suzuki_ramp_t master_volume;        /* 0x94; value is volume byte << 24 */
    suzuki_ramp_t pitch_shift;          /* 0xa0 */
    suzuki_ramp_t balance_shift;        /* 0xac */
    suzuki_music_channel_t channels[1]; /* 0xb8; channel_count entries */
} suzuki_music_t;
typedef char suzuki_music_channels_must_be_at_0xb8[(sizeof(suzuki_music_t) == 0xb8 + 0x160) ? 1 : -1];

/* Loaded SED/SMD sound block ("feds" VFX resource), kept by the Suzuki driver
 * in a singly linked list through next (SuzukiAppendVFXSMD). Play VFX SMD
 * (0x80015c38) matches id and indexes channel_offsets. */
typedef struct main_sound_resource {
    u8 unknown_00[0xa];
    u16 id;                           /* 0x0a */
    u16 volume_offset;                /* 0x0c; offset of the per-sound u8 volume table */
    u8 unknown_0e[2];                 /* 0x0e */
    struct main_sound_resource* next; /* 0x10 */
    u16 channel_offsets[1];           /* 0x14 */
} main_sound_resource_t;

/* Driver SPU state at 0x80037020: the common attributes passed to LIBSPU,
 * the volumes they and g_main_sound_reverb_attr (0x80037008) are built from,
 * and two volume ramps stepped by the root-counter handler (0x800149dc). The
 * reverb attributes are a separate object: every user addresses them from
 * their own base, not from this record's. */
typedef struct suzuki_spu_state {
    SpuCommonAttr common;     /* 0x00 0x80037020 */
    s16 music_volume;         /* 0x28 0x80037048; applied to common.mvol */
    s16 cd_volume;            /* 0x2a 0x8003704a; applied to common.cd.volume */
    s16 reverb_depth;         /* 0x2c 0x8003704c; applied to g_main_sound_reverb_attr.depth */
    s16 sound_type;           /* 0x2e 0x8003704e; Put Sound Type argument */
    suzuki_ramp_t music_ramp; /* 0x30 0x80037050 */
    suzuki_ramp_t cd_ramp;    /* 0x3c 0x8003705c */
} suzuki_spu_state_t;
typedef char suzuki_spu_state_size_must_be_0x48[(sizeof(suzuki_spu_state_t) == 0x48) ? 1 : -1];

/* SMD opcode handler, called through g_main_smd_opcode_handlers by
 * main_smd_read_instructions with the byte after the opcode; it returns the
 * next read position. Handlers that ignore an argument may declare it
 * void*; the ABI is the same. */
typedef u8* (*suzuki_smd_handler_t)(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel);

/* Scenario-music playback state at 0x8004d95c (SCUS_942.21). current_music
 * is the Suzuki music record of the playing scenario music:
 * main_sound_stop_forced_music hands it straight to
 * SuzukiDeallocateMUSChannels, which takes a suzuki_music_t*. */
typedef struct main_sound_music_state {
    s32 scenario_track;                /* 0x00 0x8004d95c slot of the playing scenario music */
    suzuki_music_t* current_music;     /* 0x04 0x8004d960 */
    suzuki_smd_header_t* forced_music; /* 0x08 0x8004d964 SMD data of the playing music */
    s32 volume;                        /* 0x0c 0x8004d968 */
} main_sound_music_state_t;

/* Music slot table at 0x8004d96c. Slots are 1-based: slots 1-2 hold the
 * scenario music opened by main_sound_open_music_into_free_slot, and tune t
 * (1-5, MUSIC_45..49.SMD) lives in slot 2 + t, so smd[3..7] (0x8004d998) hold
 * the five permanent tunes that main_sound_open_generic_sfx loads. Both
 * arrays are indexed by slot, so slot 7's handle is the word smd[0].
 * Evidence: 0x80043de0 indexes handles at 0x8004d978 + 4t and smd at
 * 0x8004d994 + 4t from a 0x8004d96c base; 0x80043a90 reads smd at
 * 0x8004d98c + 4*track and the handle 0x1c bytes below it. */
typedef struct main_sound_music_slots {
    s32 tune;                    /* 0x00 0x8004d96c pending tune (id | 0x80) or playing tune id */
    s32 handles[7];              /* 0x04 0x8004d970 Suzuki music handles; [0] is the last opened slot */
    suzuki_smd_header_t* smd[8]; /* 0x20 0x8004d98c loaded SMD data per slot */
} main_sound_music_slots_t;

/* The music state and slot table are one object: main_sound_open_generic_sfx
 * derives the slot words from the &state.forced_music base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct main_sound_music {
    main_sound_music_state_t state; /* 0x00 0x8004d95c */
    main_sound_music_slots_t slots; /* 0x10 0x8004d96c */
} main_sound_music_t;

/* Sound-effect ids passed to main_sound_play_sfx and the module queues that
 * feed it (wldcore_sound_play_effect, world_sound_dispatch_effect and the
 * g_*_sound_*effect_id globals); every path reaches main_sound_play_sfx, so
 * this is one id space. 1/2/3/5 are the Confirm, Cancel, Cursor Move and
 * Invalid sounds; the rest are named from their users. Other ids (camera
 * rotation, landing, weather, ...) stay literal until named. */
typedef enum main_sfx {
    MAIN_SFX_CONFIRM = 1,
    MAIN_SFX_CANCEL = 2,
    MAIN_SFX_CURSOR_MOVE = 3,
    MAIN_SFX_INVALID = 5,
    MAIN_SFX_PAGE_SWITCH = 6, /* L1/R1 unit and page switches */
    MAIN_SFX_UNEQUIP = 7,     /* remove equipment/abilities */
    MAIN_SFX_DISMISS = 9,     /* dismiss unit */
    MAIN_SFX_EQUIP = 0xa,     /* equip, set ability, best fit */
    /* Paired zoom sounds: battle_camera_toggle_zoom and battle_camera_zoom_map
     * play 0x11 with zoom action 4 and 0xe with action 2; the in/out reading
     * follows the zoom-target comment in battle_camera_toggle_zoom. The
     * ATTACK deploy screen also plays 0x11 when a placed unit is moved. */
    MAIN_SFX_CAMERA_ZOOM_IN = 0xe,
    MAIN_SFX_CAMERA_ZOOM_OUT = 0x11,
    MAIN_SFX_WINDOW_OPEN = 0x12,      /* help, message or text window opens */
    MAIN_SFX_TEXT_PAGE = 0x2d,        /* dialogue/scroll window advances a page */
    MAIN_SFX_CARD_ERROR = 0x30,       /* memory-card load, save or scan failure */
    MAIN_SFX_CAMERA_TILT = 0x31,      /* battle_camera_handle_tilt_request */
    MAIN_SFX_TEXT_GLYPH = 0x73,       /* typewriter tick after a dialogue glyph */
    MAIN_SFX_SHOP_TRANSACTION = 0x97, /* shop buy/sell confirmed */
} main_sfx_e;

/* Sector/size pairs for scenario and permanently loaded music SMD files. */
typedef struct main_sound_smd_file {
    s32 sector;
    s32 size;
} main_sound_smd_file_t;

/* Elapsed play time filled by main_smd_get_play_time. The root counter runs
 * at 240 Hz; music->ticks counts those ticks in 1/256 units. */
typedef struct suzuki_play_time {
    u32 tick_24;  /* 0x00; music->tick_24 */
    u16 fraction; /* 0x04; 1/240 s */
    u16 seconds;  /* 0x06 */
    u16 minutes;  /* 0x08 */
} suzuki_play_time_t;

extern u8 g_main_sound_env_sed_data[];
extern u8 g_main_sound_system_sed_data[];
extern volatile u32* g_spu_delay_reg_pointer;
extern volatile u32* volatile g_spu_dpcr_pointer;

/* Driver tables (read-only data 0x80028b0c-0x8002a8d8). */
extern suzuki_smd_handler_t g_main_smd_opcode_handlers[0x80];      /* 0x80028b0c; opcodes 0x80-0xff */
extern u8 g_main_smd_opcode_lengths[0x80];                         /* 0x80028d0c; opcode plus parameters */
extern u8 g_main_smd_note_durations[228];                          /* 0x80028d8c; ticks per note byte */
extern u8 g_main_smd_note_semitones[228];                          /* 0x80028e70; note byte / 19 */
extern suzuki_modulator_step_t g_main_smd_modulator_waveforms[16]; /* 0x80028f54 */
extern u8 g_main_smd_key_octaves[120];                             /* 0x80028fe8; key / 12 */
extern u8 g_main_smd_key_semitones[120];                           /* 0x80029060; key % 12 */

/* 0x800290d8: SPU pitch for semitone s and fine step f at index (s << 8) + f
 * (octave 6 reference); main_smd_calculate_pitch indexes it flat. */
extern u16 g_main_smd_pitch_table[12 * 256];

/* Driver globals (0x800329f0-0x80032a68, gp = 0x800329bc). The heap globals
 * are only ever reached $gp-relative, by 0x8001423c-0x8001442c. */
extern s16 g_main_sound_sfx_channel_count;                /* SFX request mode; the Play Sound wrappers store 2 */
extern suzuki_heap_block_t* g_main_sound_heap_arena;      /* 0x800329f8 */
extern suzuki_heap_block_t* g_main_sound_heap_blocks;     /* 0x800329fc */
extern main_sound_resource_t* g_main_sound_resource_list; /* 0x80032a00 */
extern s16* g_main_sound_spu_transfer_status_records;     /* SPU transfer status records, 16 bytes each */
extern u32 g_main_sound_music_key_off_voices;             /* voices queued for key-off by stopped music */
extern u32 g_main_mask_exclusion;                         /* 0x80032a0c; voices reserved by SFX */
extern u32 g_main_sound_sfx_restart_channels;             /* last SFX voice search: channel mask */
extern u32 g_main_sound_sfx_restart_voices;               /* last SFX voice search: voice mask */
extern s32 g_main_smd_random_state;                       /* 0x80032a18 */
extern s16 g_main_sound_sfx_instrument;
extern u32 g_main_sound_sfx_key_off_voices;        /* SFX voices pending key-off */
extern u32 g_main_sound_tick_count;                /* root-counter tick count; odd ticks step the ramps */
extern u16 g_main_sound_spu_transfer_status_index; /* index into D_80032A04 */
extern u32 g_main_sound_heap_size;                 /* 0x80032a38 */
extern CdlATV g_main_sound_cd_mix;                 /* 0x80032a3c; written by Put Sound Type */

/* g_main_sound_cd_mix val1 (CD left to SPU right) and val3 (CD right to SPU
 * left) as their own symbols: main_sound_put_type stores them through their
 * own `lui`, which field stores of g_main_sound_cd_mix do not reproduce. */
extern u8 g_main_sound_cd_mix_left_to_right;
extern u8 g_main_sound_cd_mix_right_to_left;
extern s16 g_main_sound_decoded_data_result;        /* SPU transfer controller state */
extern suzuki_waveset_t* g_main_sound_waveset_list; /* 0x80032a44 */
extern s32 g_main_sound_spu_event;                  /* 0x80032a48; OpenEvent handle of main_sound_spu_event_handler */
extern u16 g_main_sound_voice_update_requests;      /* voice-mask updates: bit 0 pitch LFO, 1 noise, 2 reverb */
extern suzuki_music_t* g_main_sound_active_music_list; /* 0x80032a50 */

/* Driver status: bit 0 root-counter event enabled, 0x10-0x40 SPU transfer
 * in progress (cleared by the transfer callback), 0x700 sound type, 0x1000
 * SFX enabled, 0x2000 apply the sound type at start-up, 0x8000 initialised. */
extern u16 g_main_sound_driver_flags;
extern s32 g_main_root_counter_2_event;        /* 0x80032a5c; OpenEvent handle of main_sound_root_counter_2_handler */
extern suzuki_music_t* g_main_sound_sfx_music; /* 0x80032a60; eight channels on SPU voices 16-23 */
extern u8* g_main_sound_heap_end;              /* 0x80032a64 */
extern SpuReverbAttr g_main_sound_reverb_attr; /* 0x80037008 */
extern suzuki_spu_state_t g_main_sound_spu_state; /* 0x80037020 */
extern u8 g_main_sound_heap_memory[];             /* 0x800370bc; the Suzuki heap arena */
extern u8 g_main_sound_spu_malloc_table[];        /* 0x800408e0; SpuInitMalloc(6, ...) records */
extern main_sound_music_t g_main_sound_music;
extern main_sound_smd_file_t g_main_sound_scenario_smd_files[];
extern main_sound_smd_file_t g_main_sound_permanent_smd_files[5];
extern s32 g_main_sound_weather_sfx_id;
void main_sound_set_sfx_echo(int sound_id, int echo);
void main_sound_stop_weather_sfx_music(void);
void main_sound_pause_tracked_sfx(void);
s32 main_sound_get_current_scenario_music(void);
s32 main_sound_get_music_handle(s32 slot);

/* Verified SCUS scenario-music and tune wrappers. Include this header in
 * their definitions as well as callers so ABI declarations cannot drift. */
s32 main_sound_open_and_play_music(s32 scenario, s32 slot);
void main_sound_open_generic_sfx(void);
s32 main_sound_open_music_into_free_slot(s32 scenario);
s32 main_sound_open_music_into_slot(s32 scenario, s32 slot);
s32 main_sound_open_scenario_smd_files(s32 scenario);
void main_sound_play_sfx(s32 sound_id);
void main_sound_play_weather_sfx(s32 sound_id);
void main_sound_play_sfx_find_channel(s32 sound_id);
s32 main_sound_play_tune(s32 tune);
s32 main_sound_poll_scenario_smd_load(void);
s32 main_sound_set_current_music_balance_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_pitch_shift_timed(s32 value, s32 time);
s32 main_sound_set_current_music_target(s32 volume, s32 time);
s32 main_sound_set_current_music_tempo_scale_timed(s32 value, s32 time);
void main_sound_set_current_music_track(s32 track);
s32 main_sound_set_current_music_volume(s32 volume, s32 time);
s32 main_sound_set_current_music_volume_timed(s32 time);
s32 main_sound_set_tune_volume(s32 volume);
s32 main_sound_stop_forced_music(void);

/* battle_map_step_init_sequence calls this without loading $a0, but the body reads sound_id. */
void main_sound_stop_sfx(int sound_id);
s32 main_sound_switch_music_track(s32 slot, s32 volume, s32 fade);
void main_sound_unload_current_scenario_music(void);
s32 main_sound_unload_scenario_mus(s32 slot);
void main_sound_unload_scenario_music_and_tunes(void);
s32 main_sound_update_tunes(void);
s32 main_sound_wait_for_scenario_smd_load(s32 scenario);

/* Suzuki driver entry points. The music handle passed by the game wrappers
 * is the suzuki_music_t* that SuzukiPutPlaySMD returns. */
void SuzukiAppendVFXSMD(main_sound_resource_t* resource);
void main_sound_remove_vfx_resource(main_sound_resource_t* resource);
void SuzukiCalcMusVolChange(suzuki_music_t* music, s16 volume, s16 time);
void SuzukiDeallocateMUSChannels(suzuki_music_t* music);
u32 SuzukiGetActiveChannels(suzuki_music_t* music);
u32 SuzukiGetMusicPlaying(suzuki_music_t* music);
suzuki_music_t* SuzukiPutPlaySMD(suzuki_smd_header_t* smd);
void SuzukiPlaySound1(int sound_id);
void SuzukiPlaySound2(int sound_id);
void SuzukiPlaySoundFindChannel(s32 sound_id);
void SuzukiSetSfxEcho(int sound_id, s16 echo);
void SuzukiSPUInitialiser(s32 flags);
void SuzukiTurnOffAllMusic(void);
void SuzukiUnloadMUS(suzuki_music_t* music);

/* MUS lifecycle, playback control and queue (0x800120f4-0x80014200). */
void main_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade);
void main_smd_resume_music(suzuki_music_t* music, s16 volume, s16 fade);
void main_smd_pause_music(suzuki_music_t* music);
void main_smd_stop_marked_music(void);
void main_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd);
void main_smd_set_tempo_scale(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_pitch_shift(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_balance_shift(suzuki_music_t* music, s16 value, s16 time);
void main_smd_set_channel_mute_mask(suzuki_music_t* music, u32 mask);
void main_smd_set_channel_select(suzuki_music_t* music, u8 value);
void main_smd_get_play_time(suzuki_music_t* music, suzuki_play_time_t* time);
u16* main_smd_get_min_loop_count(suzuki_music_t* music);
u8* main_smd_get_music_filename(suzuki_music_t* music);
void main_smd_dispatch_snapshot(suzuki_music_t* music, s32 mode);
void main_smd_discard_snapshot(suzuki_music_t* music);
void main_smd_save_snapshot(suzuki_music_t* music);
void main_smd_restore_snapshot(suzuki_music_t* music);
void main_smd_set_stop_bar(suzuki_music_t* music, u16 value);
u32 main_smd_retrigger_held_voices(suzuki_music_t* music);
u32 main_smd_get_held_voices(suzuki_music_t* music);
void main_smd_transfer_music_data(suzuki_music_t* music);
void main_smd_init_music_header(suzuki_music_t* music);
void main_smd_init_channels(suzuki_music_t* music);
void main_smd_free_snapshots(suzuki_music_t* music);
void main_smd_copy_snapshot(suzuki_music_t* music, suzuki_music_t* snapshot);
void main_smd_insert_music(suzuki_music_t* music);
s32 main_smd_remove_music(suzuki_music_t* music);
void main_smd_get_instrument_attr(SpuVoiceAttr* attr, s16 index);
void main_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music);

/* SFX (0x800124cc-0x80013f74). The SFX music record has eight channels on
 * SPU voices 16-23; the low byte of a start_sfx channel id is the first
 * channel and its high byte the steal priority. */
void main_sound_set_sfx_enabled(s32 enabled);
void main_sound_play_sfx_in_channel(s32 sound_id, s32 channel);
void main_sound_play_sfx_with_settings_find_channel(s32 sound_id, s32 volume, s32 balance);
void main_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance);
void main_sound_play_2_sfx(s32 first, s32 second);
void main_sound_play_4_sfx(s32 first, s32 second, s32 third, s32 fourth);
void main_sound_stop_resource_sfx(main_sound_resource_t* resource);
void main_sound_stop_sfx_channels(s32 sound_id);
void main_sound_stop_sfx_channel_pair(s32 index);
void main_sound_release_sfx_voices(suzuki_music_t* music, u32 channel_mask, u32 voice_mask);
void main_sound_set_sfx_balance(s32 sound_id, s32 balance);
u32 main_sound_get_sfx_channels(s32 sound_id);
s32 main_sound_find_sfx_voice(s32 sound_id, s32 voice_count);
suzuki_music_t* main_sound_init_sfx_music(void);
void main_smd_init_sfx_music_header(suzuki_music_t* music);
void main_sound_start_sfx(s16 channel_id, s32 sound_id, s16 volume, s16 balance);
void main_sound_stop_all(void);

/* Suzuki heap (0x8001423c-0x80014544) and SPU RAM wrappers. */
void main_sound_init_heap(void* arena, u32 size);
void* main_sound_alloc(u32 size);
void main_sound_free(void* payload);
s32 main_sound_get_largest_free_block(void);
void main_sound_copy_memory(void* destination, void* source, s32 size);
void main_sound_clear_memory(void* destination, s32 size);
s32 main_sound_alloc_spu_ram(s32 size);
s32 main_sound_free_spu_ram(u32 addr);

/* Root-counter tick (0x80014590-0x80015324, 0x80017118-0x800178e4). The
 * per-channel passes take (music, first channel, channel count). */
s32 main_sound_root_counter_2_handler(void);
void main_sound_update_voice_attrs(void);
void main_sound_flush_key_on(void);
void main_sound_flush_key_off(void);
void main_smd_step_ramp(suzuki_ramp_t* ramp);
void main_sound_calculate_pitch_lfo_voices(void);
void main_sound_calculate_noise_voices(void);
void main_sound_calculate_reverb_voices(void);
void main_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_read_instructions(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_update_modulators(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
void main_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count);
s16 main_smd_calculate_pitch(s16 key);
void main_smd_modulator_reset(suzuki_modulator_t* modulator);
void main_smd_modulator_deactivate(suzuki_modulator_t* modulator);
s32 main_smd_modulator_calculate_step(s32 depth, s16 speed, s16 waveform);
s32 main_smd_modulator_step_square(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_alternating(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_triangle(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_random(suzuki_modulator_t* modulator);
s32 main_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator);
void main_smd_seed_random(s32 value);
s32 main_smd_random(void);

/* Reprograms an SMD channel with an instrument of its waveset. */
void main_smd_set_instrument(s32 instrument, suzuki_music_channel_t* channel);
void main_smd_set_note_flags2_all_channels(u16 mask, suzuki_music_t* music);
void main_smd_force_channel_func(suzuki_music_t* music, u16 mask);

/* Driver set-up, wavesets, volume and SPU transfers (0x80017920-0x800186c4). */
void main_sound_quit(void);
suzuki_waveset_t* main_sound_load_waveset(suzuki_waveset_t* file);
void main_sound_free_wavesets(void);
void main_sound_free_waveset(suzuki_waveset_t* waveset);
suzuki_waveset_t* main_sound_find_waveset(s16 id);
void main_sound_set_type(s32 type);
s32 main_sound_get_type(void);
void main_sound_put_type(s16 level);
void main_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback);
void main_sound_set_master_volume(s16 volume, s16 time);
void main_sound_set_cd_volume(s16 volume, s16 time);
void main_sound_set_cd_reverb(s32 reverb, s32 mix);
void main_sound_commit_volume_change(void);
void main_sound_set_vol_balance(s32 volume, SpuVolume* volume_out, u8 mode);
void main_sound_transfer_spu_data(u32 spu_address, void* data, u32 size, s32 mode);
void main_suzuki_spu_callback_func(void);
s32 main_sound_spu_event_handler(void);
s16 main_sound_wait_for_spu_transfer(s32 flags);
void main_sound_replay_weather_sfx(void);

/* Suzuki key-on/key-off flushes. The driver compiled them both as functions
 * and inline in its root-counter handler, so their bodies live here once. */
/* Keys on the voices queued in each playing MUS record's key_on_mask; voices
 * reserved or pending release by SFX (g_main_mask_exclusion, g_main_sound_sfx_key_off_voices) only
 * for the SFX record (status bit 1). main_sound_flush_key_on (0x80014818) is
 * this body alone; the root-counter handler (0x800149dc) contains it inline. */
static inline void main_sound_flush_key_on_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;

    voices = 0;
    allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
    music = g_main_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & 2) != 0)
                voices |= music->key_on_mask;
            else
                voices |= allowed & music->key_on_mask;
            music->key_on_mask = 0;
        }
        music = music->next;
    }

    if (voices != 0)
        SpuSetKey(1, voices);
}

/* Keys off the voices queued in each playing MUS record's key_off_mask (same
 * SFX filter as the key-on flush) plus the voices of stopped music
 * (g_main_sound_music_key_off_voices) and the SFX voices pending release (g_main_sound_sfx_key_off_voices), which first
 * get a fast release (rate 6, mode 3). main_sound_flush_key_off (0x800148b4)
 * is this body alone; the root-counter handler contains it inline. */
static inline void main_sound_flush_key_off_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;
    u32 pending;
    s32 voice;

    allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
    voices = (allowed & g_main_sound_music_key_off_voices) | g_main_sound_sfx_key_off_voices;
    music = g_main_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & 2) != 0)
                voices |= music->key_off_mask;
            else
                voices |= allowed & music->key_off_mask;
            music->key_off_mask = 0;
        }
        music = music->next;
    }

    pending = (allowed & g_main_sound_music_key_off_voices) | g_main_sound_sfx_key_off_voices;
    if (pending != 0) {
        for (voice = 23; voice >= 0; voice--) {
            if ((pending & (1 << voice)) != 0)
                SpuSetVoiceRRAttr(voice, 6, 3);
        }
        g_main_sound_sfx_key_off_voices = 0;
        g_main_sound_music_key_off_voices = 0;
    }

    if (voices != 0)
        SpuSetKey(0, voices);
}

/* status */
/* main_status_change_unit, 0x8005e6cc: OR, AND-not, or replacement of
 * inflicted_status, followed by merging status_sets.innate into status_sets.current. */
typedef enum main_status_change {
    MAIN_STATUS_ADD = 0,
    MAIN_STATUS_REMOVE = 1,
    MAIN_STATUS_REPLACE = 2
} main_status_change_e;

extern status_infliction_data_t g_main_status_infliction_data[];
extern const status_effect_data_t g_main_status_effect_data[BATTLE_STATUS_COUNT];

/* Byte-stride alias of g_main_status_effect_data[0].flags_1. The target uses
 * this address directly while walking the 0x10-byte records. */
extern u8 g_main_status_check_sets[MAIN_STATUS_CHECK_SET_COUNT][BATTLE_STATUS_BYTE_COUNT];
void main_status_init_check_data(void);
void main_status_change_unit(battle_stats_t* unit, s32 status_set, u8 status_flag, s32 change_type);
void main_status_init_ct(battle_stats_t* unit);
void main_status_init_unit(battle_stats_t* unit);
void main_status_set_action_state(battle_stats_t* unit, u8 action_state);
s32 main_status_set_ct(battle_stats_t* unit, s32 status_id, s32 removing);
void main_status_store_current(battle_stats_t* unit);
void main_status_update_unit_flags_and_ct(battle_stats_t* unit);
s32 main_status_find_action_highest_order_effect(const u8* action);

/* system */
/* 64 allocation tags, one per 0x800-byte block in the game arena. */
extern u8 g_main_heap_game_allocator_table[64];
extern u8 g_main_heap_smd_allocator_table[16];
extern u8 g_main_heap_smd_base[];
extern game_options_t g_main_game_options;
extern volatile game_options_t g_main_game_options_defaults;
extern s32 g_main_system_frontend_world_result;
extern u32 g_main_system_game_flow_state;
extern u32 g_main_system_game_loop_stack_pointer;
extern u8 g_main_system_go_straight_to_battle;
extern int g_main_system_graphics_error;
extern s32 g_main_system_play_time_frames;
extern s32 g_main_system_play_time_hours;
extern s32 g_main_system_play_time_minutes;
extern s32 g_main_system_play_time_seconds;
extern u32 g_main_system_flags;
extern s32 g_main_system_session_frames;

/* The packed-word view of the options record; g_main_game_options names the
 * same address under its union type. The scalar name is retained because GCC
 * otherwise coalesces the saved-word load with the following bitfield writes
 * and changes the target instruction order. */
extern u32 g_main_game_options_raw;

/* Same address as g_main_system_flags (0x8004d950). The target re-reads the
 * flags in both arms after the (flags & 0xc) == 4 test; reading the test
 * through this second link name keeps GCC from reusing the tested load, where
 * a volatile declaration also reschedules the argument setup. */
extern s32 g_main_system_flags_alias;
extern s32 g_main_boot_startup2_has_run;
extern u32 g_main_boot_squaresoft_logo_image_words[];
extern u8* g_main_heap_high_overlay_load_address;
extern u8* g_main_heap_low_overlay_load_address;
extern u8* g_main_heap_world_overlay_load_address;
void main_noop_800449ec(void);

/* 0x800449f8: a return-only hook. Main and overlay callers supply two words,
 * but neither their original meaning nor the hook's original name is proven. */
void main_noop_800449f8(s32 category, s32 code);
void main_overlay_call_battle_entrypoint(void);
void main_overlay_exec_battle_bin(void);
int main_overlay_exec_open_bin_main_loop(int mode);
void main_overlay_exec_open_bin_ending(void);
int main_overlay_open_world_and_wldcore(int load_world);
void main_overlay_open_world_bin(s32 mode);
void main_restore_game_loop_stack_pointer(u32* source);
void main_system_handle_animation_exception(int error_code);
void main_system_handle_battle_load_exception(s32 value);
void main_system_handle_malloc_exception(s32 allocator_id, s32 error_code);
void main_system_handle_pointer_exception(int error_code);
void main_system_reset_game(void);
void main_system_run_game_loop(void);
void main_system_stop_display_and_audio_for_game_reset(void);
void main_system_store_stack_pointer(u32* destination);
s32 main_util_roll_pass_fail(s32 maximum, s32 threshold);
void main_boot_run_startup(void);
void main_boot_reset_game_state(void);
void main_system_handle_draw_sync_callback(void);
void main_system_handle_vsync_callback(void);
void main_boot_build_and_draw_sceap_logo(void);
void main_boot_build_and_draw_squaresoft_logo(void);
int main_boot_draw_sceap_logo(void* otag, u32* image);
int main_boot_draw_squaresoft_logo(u32 otag, u32* image);
void main_boot_fade_out_squaresoft_logo(void);
void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void main_util_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);
void* main_heap_alloc(u32 size);
void* main_heap_alloc_smd(u32 size);
void main_heap_call_free(void* allocation);
void main_heap_clear_game_allocator_table(void);
s32 main_heap_free(void* allocation);
u8* main_heap_reserve_at(void* allocation, u32 size);
s32 main_heap_free_smd(void* allocation);
void main_util_clear_byte_data(void* destination, s32 count);

/* SCUS byte loops at 0x8005e254 (copy) and 0x8005e644 (clear), not SDK
 * memcpy/memset. Copy takes source first;
 * both routines do nothing for a nonpositive signed byte count. */
void main_util_copy_byte_data(const void* source, void* destination, s32 count);
void* game_malloc(s32 byte_length);
void main_util_copy_action_data(const u8* source, u8* destination);
void main_heap_clear_smd_allocator_table(void);
void main_noop_80014544(void);
void main_system_enable_root_counter_2_evcb(void);
void main_system_disable_root_counter_2_evcb(void);
void main_util_copy_bytes(const void* source, void* destination, int count);

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

typedef struct main_unit_generation_raw_stat_variance {
    u8 raw_stats[UNIT_RAW_STAT_COUNT];
} main_unit_generation_raw_stat_variance_t;
typedef char
    main_unit_generation_raw_stat_variance_size_must_be_5[(sizeof(main_unit_generation_raw_stat_variance_t) == 5) ? 1
                                                                                                                  : -1];

/* Values accepted by main_status_set_action_state. */
typedef enum main_unit_action_state {
    MAIN_UNIT_ACTION_STATE_NONE = 0,
    MAIN_UNIT_ACTION_STATE_CHARGING = 5,
    MAIN_UNIT_ACTION_STATE_JUMPING = 6,
    MAIN_UNIT_ACTION_STATE_DEFENDING = 7,
    MAIN_UNIT_ACTION_STATE_PERFORMING = 8,
    MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING = 0xff
} main_unit_action_state_e;

extern u8 g_highest_party_level;
extern u8 g_party_slot_base;
extern s32 g_main_entd_current_event_id;
extern entd_file_t* g_main_entd_set;
extern party_data_t g_main_party_data[PARTY_ROSTER_SLOT_COUNT];
extern main_unit_generation_base_data_t g_main_unit_generation_base_data[MAIN_UNIT_TYPE_COUNT];
extern main_unit_generation_raw_stat_variance_t g_main_unit_generation_raw_stat_variance[MAIN_UNIT_TYPE_COUNT];
extern u32 g_main_unit_name_uses_world_text;
void main_unit_update_formation_stats(battle_stats_t* unit);

/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* main_entd_get_encounter(void);
int main_entd_open_file(void);
void main_entd_init_event_unit_data(int event_id);
void main_party_clear_all(void);
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
s32 main_party_find_free_slot(s32 save_formation, u8* palette);
s32 main_party_find_slot_by_sprite_set(s32 sprite_set);

/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 main_party_generate_unit(s32 unit_type);
party_data_t* main_party_get_data_pointer(s32 party_index);
void main_party_init_new_game(s32 mode);
u32 main_party_level_unit_to_target(party_data_t* party_unit, s32 level_delta);
void main_party_remove_unit(u32 index);
s32 main_party_save_unit(battle_stats_t* unit, s32 allow_guest);
void main_unit_apply_equipment_move_jump_and_name(battle_stats_t* unit);
void main_unit_apply_level_growth(battle_stats_t* unit, s32 level_down);
void main_unit_calculate_abilities(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_actual_stats(battle_stats_t* unit, s32 mode);
s32 main_unit_calculate_entd_data(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_entd_equipment(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_jobs_and_skillsets_from_entd(battle_stats_t* unit, const entd_unit_t* entd);
void main_unit_calculate_move_jump(battle_stats_t* unit, s32 keep_position);
u8 main_unit_calculate_palette_spritesheet(battle_stats_t* unit, u8* palette);

s32 main_unit_calculate_random_equipment(
    battle_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type);

u16 main_unit_calculate_rsm(battle_stats_t* unit, u16 ability_id, s32 flags, entd_unit_t* entd);
s32 main_unit_calculate_zodiac_symbol(u32 birthday);
s32 main_unit_check_level_up(battle_stats_t* unit);
void main_unit_copy_job_data(battle_stats_t* unit);
void main_unit_copy_job_growths_and_multipliers(battle_stats_t* unit);
void main_unit_copy_last_ability_ct(battle_stats_t* unit);
void main_unit_enable_rsm_flags(battle_stats_t* unit);
void main_unit_generate_base_raw_stats(u8* raw_stats, s32 unit_type);
void main_unit_generate_out_of_battle(party_data_t* party, s32 unit_type);
void main_unit_generate_party_base_raw_stats(party_data_t* party, s32 unit_type);
void main_unit_generate_raw_stats(battle_stats_t* unit);
u8 main_unit_get_spritesheet_palette(battle_stats_t* unit, u8* palette);
s32 main_unit_has_status_in_set(const battle_stats_t* unit, main_status_check_set_e status_set);
s32 main_unit_init(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle);
void main_unit_init_battle_data(battle_stats_t* unit, const party_data_t* party_unit);
s32 main_unit_init_for_battle(battle_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind);
s32 main_unit_init_job_data(battle_stats_t* unit, s32 slot, s32 use_world_text);
s32 main_unit_init_job_data_from_entd(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_init_job_levels(u16* unit_job_jp, u8* unit_job_levels);
void main_unit_init_status_and_rewards(battle_stats_t* unit, s32 clear_rewards);
void main_unit_learn_job_abilities(battle_stats_t* unit, s32 job_id, const entd_unit_t* entd);
void main_unit_refresh_stats_and_statuses(battle_stats_t* unit);
void main_unit_reset_battle_state(battle_stats_t* unit);
void main_unit_set_equipment_attributes(battle_stats_t* unit, s32 level_up_check);

/* SCUS_942.21 unit equipment/name pipeline, 0x8005ab00 and
 * 0x8005c398..0x8005cbd0. Signatures are shared with the matching definitions. */
void main_unit_set_equipment_stats(battle_stats_t* unit);
void main_unit_set_equippable_items(battle_stats_t* unit);
void main_unit_set_rsm_flag(battle_stats_t* unit, u32 ability_id);
void main_unit_store_character_names(battle_stats_t* unit);
void main_unit_store_monster_equipment(battle_stats_t* unit, const entd_unit_t* entd);
void main_unit_store_ramza_name_birthday_zodiac(battle_stats_t* unit);
void main_unit_update_stats_statuses_and_equipment(battle_stats_t* unit, s32 skip_status_check, s32 initializing);
u8 main_party_calculate_highest_level(void);
void main_unit_increase_casualty_counters(battle_stats_t* unit);
void main_unit_init_position_and_rewards(battle_stats_t* unit, entd_unit_t* entd);

/* zodiac */
/* ZODIAC.BIN sprite-table records shared by the builder and polygon helper. */
typedef struct main_zodiac_texture_position {
    s16 x;
    u16 y; /* Bits 8..11 select the texture-page row. */
} main_zodiac_texture_position_t;

typedef struct main_zodiac_screen_position {
    u16 x;
    u16 y;
} main_zodiac_screen_position_t;

typedef struct main_zodiac_sprite_frame {
    u8 u;
    u8 unknown_01;
    u8 v;
    u8 unknown_03;
    s16 width;
    s16 height;
    s16 offset_x;
    s16 offset_y;
} main_zodiac_sprite_frame_t;

typedef struct main_zodiac_scale {
    s16 x;
    s16 y;
} main_zodiac_scale_t;

typedef struct main_zodiac_draw_offset {
    u8 unknown_00[8];
    u16 x;
    u16 y;
} main_zodiac_draw_offset_t;

/* Draw state handed to SCUS Build ZODIAC.BIN (provisional layout). */
typedef struct zodiac_draw_context {
    u32* ot;       /* 0x00: ordering table entry the primitives link into */
    u8 brightness; /* 0x04: r0/g0/b0 of every sprite */
    u8 unknown_05[3];
    s32 link_primitives; /* 0x08: nonzero: link each sprite into the OT */
    s16 scale_x;         /* 0x0c: 12.12 */
    s16 scale_y;         /* 0x0e: 12.12 */
    u8 unknown_10[8];
    main_zodiac_draw_offset_t offset; /* 0x18 */
} zodiac_draw_context_t;

extern const u16 g_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];
extern POLY_FT4 g_main_zodiac_primitives[2][30];

/* gfx */
extern RECT g_main_gfx_frame_image_rect;
extern RECT g_main_gfx_frame_palette_rect;
extern RECT g_main_gfx_frame_palette_tail_rect;
extern u32 g_main_gfx_now_loading_clut[];
extern u32 g_main_gfx_now_loading_frame_counter;
extern u32 g_main_gfx_now_loading_otags[2][2];

/* Seven textured quads per screen polarity; main_gfx_add_now_loading_to_otag
 * walks the same table as a [7][sizeof(POLY_FT4)] byte array. */
extern POLY_FT4 g_main_gfx_now_loading_primitives[2][7];
extern u32 g_main_gfx_now_loading_visible;
extern main_zodiac_screen_position_t g_main_gfx_zodiac_screen_position;
extern u8 g_main_gfx_zodiac_sprite_count;
extern main_zodiac_sprite_frame_t g_main_gfx_zodiac_sprite_frames[];
extern main_zodiac_scale_t g_main_gfx_zodiac_sprite_scales[];
extern main_zodiac_texture_position_t g_main_gfx_zodiac_texture_position;
extern s32 g_main_gfx_display_buffer_index;
extern DISPENV g_main_gfx_display_envs[2];
extern DRAWENV g_main_gfx_draw_envs[2];
extern s32 g_main_gfx_loading_display_frame_counter;
extern s32 g_main_gfx_screen_polarity;
void main_gfx_add_now_loading_to_otag(u32* otag);
void main_gfx_build_now_loading(u32 visible, s32 x, s32 y);
void main_gfx_draw_now_loading_message(void);
u32* main_gfx_get_otag(void);
void main_gfx_load_efc_fnt(void);
void main_gfx_load_frame_bin_into_vram(void);
void main_gfx_load_zodiac_frame(void);
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_gfx_swap_and_clear_otag(void);
int main_gfx_swap_display_area(u32 otag);
void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_set_display_draw_new_game(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_gfx_build_now_loading_center(u32 visible);
void main_gfx_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly);
void main_gfx_call_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly);

void main_gfx_build_zodiac_sprite_polygon(POLY_FT4* poly, main_zodiac_texture_position_t* texture,
    main_zodiac_screen_position_t* base, main_zodiac_sprite_frame_t* frame, main_zodiac_scale_t* scale,
    main_zodiac_draw_offset_t* offset);

/* other */
extern volatile u8 g_fntload_clut_y_offset_immediate;
extern s32 g_font_print_enabled;
extern u8* g_main_brave_story_ages_source;
extern u8 g_main_menu_scroll_accel_delay;
extern u8 g_main_menu_scroll_slow_step;
extern u8 g_main_menu_scroll_fast_step;
extern u8 g_main_accessory_page_order[0x21];
extern u8 g_main_armor_page_order[0x25];
extern u8 g_main_helmet_page_order[0x1d];
extern u8 g_main_action_menu_types_by_skillset[0xe0];
extern u8 table[0x100];
extern s32 g_main_battle_bin_sector;
extern u8 g_main_common_menu_graphics_loaded;
extern u32 g_main_tutorial_progress_masks[];
extern u8 g_main_special_portrait_unit_id;
extern u8 g_main_calculator_primes[CALCULATOR_PRIME_COUNT];
extern u16 g_main_month_start_day_offsets[13];
extern struct battle_deployed_coords* g_main_current_formation_entry;
extern u8 g_main_crystal_learnable_abilities[];
extern u8 g_main_crystal_treasure_item_id;
extern u8 g_main_brave_story_character_ages[0x40];
extern s32 g_main_debug_display_enabled;
extern s32 g_main_replay_story_event_index;
extern u8 g_main_special_portrait_wldface_id;
extern s32 g_main_deployed_unit_map_coordinates[];
extern u8 g_main_terrain_movement_cost_tables[][64];
extern const u8 g_main_terrain_status_flags[64];
s32 get_total_equipment_quantity(s32 item_id, s32 include_equipped);
void AddPrims(u32* ot, void* first, void* last); /* follows AddPrim; AddPrims-shaped call */
s32 main_return_zero_80043708(void);

/* unnamed */
void func_800184e0(u16 value);
extern u16 D_80032A28; /* only ever stored */
extern u32 D_80032A34; /* only ever stored */
extern s16 D_80032A58; /* only ever stored */
extern s32 D_800459D8;
extern u8 D_800473A7;
extern u8 D_80057b1c;
extern u8 D_8005E950[];

#endif

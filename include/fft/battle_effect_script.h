#ifndef BATTLE_EFFECT_SCRIPT_H
#define BATTLE_EFFECT_SCRIPT_H

/* BATTLE effect playback: file loading, playback stages, the script record pool and the script interpreter. */

#include "fft/battle_effect.h"
#include "fft/battle_effect_file.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* loading and playback */
/* g_battle_effect_load_state: the steps of battle_effect_update_file_load, which loads the ability's effect file
 * while its charge secondary effect plays. */
typedef enum effect_load_state {
    EFFECT_LOAD_START = 0,            /* start reading the file */
    EFFECT_LOAD_READING = 1,          /* wait for the read */
    EFFECT_LOAD_COUNTDOWN = 2,        /* wait out the 60 frames given to a charge secondary effect */
    EFFECT_LOAD_ENDING_SECONDARY = 3, /* wait for the charge secondary effect to end */
} effect_load_state_e;

/* g_battle_effect_playback_phase: the stages of battle_effect_update_stage. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for battle_effect_play */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6, /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* target_mode of battle_effect_start_new. */
typedef enum effect_start_target {
    EFFECT_START_ON_UNIT = 0,
    EFFECT_START_ON_TILE = 1,
    EFFECT_START_ON_CASTER = 2, /* a unit that is both caster and target */
} effect_start_target_e;

/* target_mode selects the unit id or the tile coordinate pointer. */
typedef union effect_start_target_data {
    s32 runtime_id;
    const SVECTOR* tile;
} effect_start_target_data_t;
typedef char effect_start_target_data_size_must_be_4[sizeof(effect_start_target_data_t) == 4 ? 1 : -1];

/* Verified prefix only: battle_effect_replace_flags_0006 reads and writes a 16-bit value at +6.
 * Neither the full record nor the individual bit meanings are known. */
typedef struct effect_flag_prefix {
    u8 _unused_00[6];
    u16 flags;
} effect_flag_prefix_t;
typedef char effect_flag_prefix_size_must_be_8[(sizeof(effect_flag_prefix_t) == 8) ? 1 : -1];

/* Action/reaction ability IDs index directly; event IDs map after those rows. */
extern s16 g_battle_effect_ability_files[];
extern effect_file_header_t* g_battle_effect_data_ptrs[EFFECT_FILE_ID_COUNT];
extern file_extent_t g_battle_effect_disc_entries[EFFECT_FILE_ID_COUNT];
extern s16 g_battle_effect_load_countdown;
extern u8 g_battle_effect_load_state;      /* effect_load_state_e */
extern s32 g_battle_effect_playback_phase; /* effect_playback_phase_e */

void battle_effect_call_play(void);
void battle_effect_cancel_playback_request(void);
s32 battle_effect_get_playback_phase(void);
s32 battle_effect_update_file_load(void);
void battle_effect_init_system(void);
s32 battle_effect_is_item_ability(s32 ability_id);
u32 battle_effect_ability_has_no_file(s32 ability_id);
void battle_effect_play(void);
void battle_effect_replace_flags_0006(struct effect_flag_prefix* object, s32 value);
void battle_effect_reset_subsystems(void);
void battle_effect_set_ability_animation(u32 packed, s32 ability, effect_secondary_init_t* source);
void battle_effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 battle_effect_poll_file_load(void);
s32 battle_effect_update_cycle(void);

/* script records */
extern effect_record_t g_battle_effect_state_records[EFFECT_STATE_RECORD_CAPACITY + 1];
/* Heads of the allocated and available record lists, chained through effect_record_t.next_index.
 * The available head is unsigned; the allocated head matches retail only when declared s16. */
extern s16 g_battle_effect_allocated_record_head;
extern u16 g_battle_effect_available_record_head;
extern s16 g_battle_effect_current_record_index;
extern s16 g_battle_effect_root_record_index;

void battle_effect_init_state_records(void);

/* script */
/* What an opcode handler returns to battle_effect_run_block. */
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,    /* stop for this frame */
    EFFECT_SCRIPT_RESULT_CONTINUE = 1, /* run the next instruction */
    EFFECT_SCRIPT_RESULT_ABORT = 2,    /* the script has ended: free the record */
} effect_script_result_e;

/* Written by effect-script opcode 05; no game source reads it. */
extern u16 g_battle_effect_script_opcode_05_value;
extern effect_callback_t g_battle_effect_callback_slots[];
extern effect_script_result_e (*g_battle_effect_instructions[120])(effect_record_t*);
extern effect_callback_t g_battle_effect_native_callbacks[EFFECT_NATIVE_CALLBACK_COUNT];

effect_script_result_e battle_effect_code_script_04_end(effect_record_t* unused_record);
effect_script_result_e battle_effect_code_script_2e_invalid(effect_record_t* unused_record);

#endif

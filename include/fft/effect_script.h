#ifndef EFFECT_SCRIPT_H
#define EFFECT_SCRIPT_H

/* BATTLE effect playback: file loading, playback stages, the script record pool and the script interpreter. */

#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/file.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* loading and playback */
/* g_effect_load_state: the steps of effect_file_update_load, which loads the ability's effect file
 * while its charge secondary effect plays. */
typedef enum effect_load_state {
    EFFECT_LOAD_START = 0,           /* start reading the file */
    EFFECT_LOAD_READING = 1,         /* wait for the read */
    EFFECT_LOAD_COUNTDOWN = 2,       /* wait out the 60 frames given to a charge secondary effect */
    EFFECT_LOAD_ENDING_SECONDARY = 3 /* wait for the charge secondary effect to end */
} effect_load_state_e;

/* The stages of effect_update_stage, kept in g_effect_playback_phase. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for effect_play */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6 /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* The target_mode of effect_start_new. */
typedef enum effect_start_target {
    EFFECT_START_ON_UNIT = 0,
    EFFECT_START_ON_TILE = 1,
    EFFECT_START_ON_CASTER = 2 /* a unit that is both caster and target */
} effect_start_target_e;

/* target_mode selects the unit id or the tile coordinate pointer. */
typedef union effect_start_target_data {
    s32 runtime_id;
    const SVECTOR* tile;
} effect_start_target_data_t;
typedef char effect_start_target_data_size_must_be_4[sizeof(effect_start_target_data_t) == 4 ? 1 : -1];

void effect_init_system(void);
void effect_reset_subsystems(void);
u32 effect_ability_has_no_file(s32 ability_id);
s32 effect_is_item_ability(s32 ability_id);
void effect_set_ability_animation(u32 packed, s32 ability, const effect_context_t* source);
void effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 effect_file_update_load(void);
s32 effect_file_poll_load(void);
void effect_play(void);
void effect_call_play(void);
void effect_cancel_playback_request(void);
s32 effect_get_playback_phase(void);
s32 effect_update_cycle(void);

/* script records */
extern effect_record_t g_effect_state_records[EFFECT_STATE_RECORD_CAPACITY + 1];

void effect_script_init_state_records(void);

/* script */
/* What an opcode handler returns to effect_script_run_block. */
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,    /* stop for this frame */
    EFFECT_SCRIPT_RESULT_CONTINUE = 1, /* run the next instruction */
    EFFECT_SCRIPT_RESULT_ABORT = 2     /* the script has ended: free the record */
} effect_script_result_e;

effect_script_result_e effect_script_opcode_04_end(effect_record_t* unused_record);
effect_script_result_e effect_script_opcode_2e_invalid(effect_record_t* unused_record);

#endif

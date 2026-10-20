#ifndef EFFECT_SCRIPT_H
#define EFFECT_SCRIPT_H

/* BATTLE effect playback: file loading, playback stages, the script record pool and the script interpreter. */

#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/file.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* loading and playback */
/* The stages of stage_update, kept in g_effect_playback_phase. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for effect_playback_start */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6 /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* The target_mode of effect_request_start. */
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

void effect_system_init(void);
void effect_subsystems_reset(void);
u32 effect_ability_has_no_file(s32 ability_id);
s32 effect_is_item_ability(s32 ability_id);
void effect_ability_animation_set(u32 packed, s32 ability, const effect_context_t* source);
void effect_request_start(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 effect_file_load_update(void);
s32 effect_request_load_poll(void);
void effect_playback_start(void);
void effect_request_play(void);
void effect_playback_request_cancel(void);
s32 effect_playback_phase_get(void);
s32 effect_playback_update(void);

/* script records */
extern effect_record_t g_effect_state_records[EFFECT_STATE_RECORD_CAPACITY + 1];

void effect_script_instances_init(void);

/* script */
/* What an opcode handler returns to script_block_run. */
typedef enum effect_script_result {
    EFFECT_SCRIPT_RESULT_YIELD = 0,    /* stop for this frame */
    EFFECT_SCRIPT_RESULT_CONTINUE = 1, /* run the next instruction */
    EFFECT_SCRIPT_RESULT_ABORT = 2     /* the script has ended: free the record */
} effect_script_result_e;

effect_script_result_e effect_script_opcode_04_end(effect_record_t* unused_record);
effect_script_result_e effect_script_opcode_2e_invalid(effect_record_t* unused_record);

#endif

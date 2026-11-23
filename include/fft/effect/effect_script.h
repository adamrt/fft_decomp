#ifndef EFFECT_SCRIPT_H
#define EFFECT_SCRIPT_H

/* BATTLE effect playback: file loading, playback stages, the script record pool and the script interpreter. */

#include "fft/effect/effect.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* loading and playback */
/* The stages of update_stage, kept in g_effect_playback_phase. */
typedef enum effect_playback_phase {
    EFFECT_PLAYBACK_PHASE_IDLE = 0,
    EFFECT_PLAYBACK_PHASE_REQUESTED = 1, /* waiting for effect_playback_start */
    EFFECT_PLAYBACK_PHASE_INIT_PALETTE = 2,
    EFFECT_PLAYBACK_PHASE_START_SCRIPT = 3, /* bind the effect file's sections and start its script */
    EFFECT_PLAYBACK_PHASE_RUNNING = 4,
    EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT = 6 /* start the script again without rebinding the file */
} effect_playback_phase_e;

/* The target_mode of effect_start_request. */
typedef enum effect_start_target {
    EFFECT_START_ON_UNIT = 0,
    EFFECT_START_ON_TILE = 1,
    EFFECT_START_ON_CASTER = 2 /* a unit that is both caster and target */
} effect_start_target_e;

/* target_mode selects the unit id or the tile coordinate pointer. */
typedef union effect_start_target_data {
    s32 map_unit_id;
    const SVECTOR* tile;
} effect_start_target_data_t;
typedef char effect_start_target_data_size_must_be_4[sizeof(effect_start_target_data_t) == 4 ? 1 : -1];

void effect_init_system(void);
void effect_reset_subsystems(void);
u32 effect_ability_has_no_file(s32 ability_id);
s32 effect_is_item_ability(s32 ability_id);
void effect_playback_request(u32 packed, s32 ability, const effect_context_t* source);
void effect_start_request(s32 ability_id, s32 target_mode, effect_start_target_data_t target);
s32 effect_update_file_load(void);
s32 effect_poll_request_load(void);
void effect_playback_start(void);
void effect_play_request(void);
void effect_playback_cancel_request(void);
s32 effect_playback_get_phase(void);
s32 effect_playback_update(void);

/* script records */
extern effect_record_t g_effect_state_records[EFFECT_STATE_RECORD_CAPACITY + 1];

void effect_script_init_instances(void);

#endif

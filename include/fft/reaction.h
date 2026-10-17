#ifndef REACTION_H
#define REACTION_H

/* Reaction abilities (Blade Grasp, Counter, Hamedo, MP Switch, Distribute ...): the reaction an action provokes, the
 * Brave roll and the checks that block a reaction, and the reactions tried during the formulas. */

#include "fft/unit.h"
#include "psx/types.h"

/* Reaction behavior flags (g_ability_reaction_behavior_flags). */
typedef enum reaction_behavior_flag {
    REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY = 0x01,
    REACTION_BEHAVIOR_FLAG_ABILITY = 0x02,
    REACTION_BEHAVIOR_FLAG_TARGET_SELF = 0x04,
    REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER = 0x08,
    REACTION_BEHAVIOR_FLAG_PASSIVE = 0x10,
    REACTION_BEHAVIOR_FLAG_ACTIVE = 0x20
} reaction_behavior_flag_e;

extern s16 g_reaction_ability_id;
extern s32 g_reaction_hamedo_flag;
extern s32 g_reaction_distribute_target_count;

s32 reaction_prepare_hamedo(s32 id, s16* out_ability);
s32 reaction_prepare_next(s16* out_ability);
s32 reaction_fails_brave_roll(const unit_stats_t* unit);
s32 reaction_is_blocked(const unit_stats_t* unit);
s32 reaction_is_prevented(const unit_stats_t* unit);
void reaction_try_finger_guard(void);
void reaction_try_catch(void);

#endif

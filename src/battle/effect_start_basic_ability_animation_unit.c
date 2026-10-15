#include "fft/battle_effect.h"
#include "psx/types.h"

void effect_start_basic_ability_animation(s32 ability_id) {
    effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;
    effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[0].data.unit.runtime_id = 1;
    effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    effect_data.spectator_count = 0;
    effect_set_ability_animation(0, ability_id, &effect_data);
}

/* Starts an ability's effect outside an action, on a unit, on a tile or
 * on a unit that is also its caster. */
void effect_start_new(s32 ability_id, s32 target_mode, effect_start_target_data_t target) {
    effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = 0;

    if (target_mode == EFFECT_START_ON_UNIT) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
    } else if (target_mode == EFFECT_START_ON_TILE) {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        effect_data.targets[0].data.tile.vx = target.tile->vx;
        effect_data.targets[0].data.tile.vz = target.tile->vz;
        effect_data.targets[0].data.tile.vy = target.tile->vy;
    } else {
        effect_data.targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
        effect_data.targets[0].data.unit.runtime_id = target.runtime_id;
        effect_data.targets[0].data.unit.result_animation = EFFECT_HIT_NORMAL;
        effect_data.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = target.runtime_id;
    }

    effect_data.spectator_count = 0;
    effect_set_ability_animation(0, ability_id, &effect_data);
}

/* effect_init_data as a 0 or 1 result: the event script's EffectStart
 * instruction calls it on the main stack each frame until it returns 0. */
s32 effect_try_init_data(void) {
    /* The target loads no argument; the parameter is the $a0 value unhandled states return. */
    return effect_init_data() != 0;
}

void effect_call_play(void) {
    effect_play();
}

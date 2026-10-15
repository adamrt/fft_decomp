#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/types.h"

void battle_menu_clear_status_menu_state(void) {
    g_battle_menu_status_requested = 0;
    g_status_menu_open = 0;
}

/* Fill an init record for a secondary effect that one unit both casts and
 * receives: the caster and the single target are the unit. Every other field of
 * the record is left as the caller's stack held it. */
void effect_build_secondary_init(battle_unit_t* unit, effect_secondary_init_t* effect) {
    effect->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    effect->target_count = 1;
    effect->targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[0].data.unit.runtime_id = unit->runtime_id;
}

/* Fill a secondary-effect init record with the unit's current action targets.
 *
 * Each target block records the target's unit id and a result animation
 * derived from its action data. With no targets the first block instead holds
 * the cursor tile position. The record's target blocks form an array of up to
 * 16 entries starting at the header's first `target` block. */
void effect_build_secondary_init_from_action(battle_unit_t* unit, effect_secondary_init_t* init) {
    battle_unit_t* target;
    action_data_t* action;
    s32 i;

    init->target_count = unit->strike.target_count;
    init->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    init->used_weapon_id = unit->strike.used_weapon_id;
    if (init->target_count != 0) {
        for (i = 0; i < init->target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            init->targets[i].target_type = EFFECT_TARGET_TYPE_UNIT;
            init->targets[i].data.unit.runtime_id = target->runtime_id;
            action = &target->stats->action;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_MISSED;
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_NO_EFFECT;
                break;
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_GOLEM;
                break;
            default:
                if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_BREAK;
                } else if (action->critical != 0) {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_CRITICAL;
                } else {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_NORMAL;
                }
                break;
            }
        }
    } else {
        init->targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        init->targets[0].data.tile.vx = g_battle_cursor_x;
        init->targets[0].data.tile.vy = g_battle_cursor_z;
        init->targets[0].data.tile.vz = g_battle_cursor_y;
    }
}

/* Starts the weapon-hit secondary effect of the unit's action, in the element
 * of the used weapon; is_bow is 1 for the bow hardcoding path and 0
 * otherwise, and unused. */
void effect_set_evade_type_data_and_weapon_element(battle_unit_t* unit, s32 is_bow) {
    effect_secondary_init_t secondary_init;
    effect_build_secondary_init_from_action(unit, &secondary_init);
    effect_init_weapon_hit_secondary(
        g_main_item_weapon_data[unit->used_item_or_weapon_id].element, &secondary_init, is_bow);
}

/* Starts the projectile secondary effect of the unit's action: the stone for
 * Throw Stone, the thrown item for the Item and Throw abilities, and for
 * anything else the animation of the equipped weapon's type (an arrow for
 * bows and crossbows, none for the rest). The target retains a redundant
 * Shuriken test before checking the complete Item/Throw span. */
void effect_start_action_projectile_secondary(battle_unit_t* unit) {
    effect_secondary_init_t secondary_init;
    u16 ability_id;
    u8 animation_id;
    effect_build_secondary_init_from_action(unit, &secondary_init);
    ability_id = unit->used_ability_id;
    if (ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_STONE;
    } else {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_ITEM;
        if ((ability_id != ABILITY_ID_THROW_SHURIKEN)
            && ((u32)(unit->used_ability_id - ABILITY_ID_ITEM_FIRST)
                >= (ABILITY_ID_JUMP_FIRST - ABILITY_ID_ITEM_FIRST))) {
            animation_id = g_effect_weapon_projectile_animations[unit->equipped_weapon_type];
        }
    }
    effect_init_projectile_secondary(animation_id, &secondary_init);
}

/* Start a secondary effect on a unit as both its caster and its target, with the
 * unit's sprite palette as the effect's parameter. A unit without battle data
 * starts nothing.
 *
 * The init record is a stack local of which effect_build_secondary_init
 * sets five fields; effect_init_secondary copies the rest of its caster
 * and target blocks and its used_weapon_id as the stack left them. */
void effect_set_secondary(battle_unit_t* unit, s32 effect_type) {
    effect_secondary_init_t data;

    effect_build_secondary_init(unit, &data);
    if (unit->stats != 0) {
        effect_init_secondary(effect_type, unit->stats->sprite_palette, &data);
    }
}

void effect_set_secondary_zodiac_poof(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF);
}

void effect_set_secondary_death_smoke(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE);
}

void effect_set_secondary_level_up(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_LEVEL_UP);
}

void effect_set_secondary_venom_trap(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_VENOM_TRAP);
}

void effect_set_secondary_splash(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_SPLASH);
}

void effect_set_secondary_fall_dust(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_FALL_DUST);
}

void effect_set_secondary_teleport(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_TELEPORT);
}

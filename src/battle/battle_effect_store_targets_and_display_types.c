#include "fft/battle.h"
#include "fft/battle_ability.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Array view of battle_effect_secondary_init_t's sixteen ten-byte target
 * blocks at 0x04..0xa3. Indexing a real array member keeps the target's
 * `addu rD,record,offset` operand order; pointer arithmetic on the header's
 * single `target` block commutes it. */
typedef struct battle_effect_secondary_init_targets {
    u16 target_count;
    u8 palette_target_count;
    u8 math_skill_flag;
    union {
        struct {
            s16 target_type;
            u8 target_id;
            u8 result_animation;
        } fields;
        battle_effect_secondary_block_t block;
    } target[16];
} battle_effect_secondary_init_targets_t;

/* Build the secondary-effect init record for an ability and hand it to the
 * effect selector.
 *
 * Each of the acting unit's targets contributes a target block carrying its
 * unit id and a result animation; with no targets the first block instead
 * holds the post-action destination tile. Every other living unit that is not
 * already a target is appended to palette_target_ids. The packed
 * argument combines the ability's charge animation set with the element used
 * as the effect's sprite palette, and stays 0 while the action has no attack
 * to animate. The failed-condition path is written as its own `packed = 0`
 * block: the target keeps that block, and a pre-initialized `packed` lets GCC
 * thread the failing tests straight to the call. Items 0x4a..0x4c (the
 * elemental guns) and ability 0x189 (Ninja Ball) are hard-coded exceptions. */
void battle_effect_store_targets_and_display_types(s32 ability, battle_unit_misc_data_t* unit) {
    battle_effect_secondary_init_t init;
    battle_unit_misc_data_t* target;
    battle_unit_misc_data_t* spectator;
    battle_stats_t* stats;
    s32 i;
    s32 j;
    s32 spectators;
    u32 packed;
    u8 item;
    battle_strike_work_t* block;

    init.target_count = unit->target_count;
    init.caster.fields.target_type = 0;
    init.caster.fields.caster_id = unit->unit_id;
    save_3_u16(
        (SVECTOR*)&init.target_tile.id.tile_x, unit->target_new_x, unit->target_new_map_level, unit->target_new_y);
    block = (battle_strike_work_t*)&unit->action_18c;
    if (init.target_count != 0) {
        for (i = 0; i < init.target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(block->target_list[i]);
            ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.target_type = 0;
            ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.target_id = target->unit_id;
            {
                battle_stats_t* target_stats;

                target_stats = target->battle_data;

                if (target_stats == 0) {
                    ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.result_animation = 0;
                } else if (target_stats->action.critical != 0) {
                    ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.result_animation = 1;
                } else if (target_stats->action.miss_type == BATTLE_ACTION_MISS_TYPE_REFLECTED) {
                    ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.result_animation = 2;
                } else if (target_stats->action.miss_type != 0) {
                    ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.result_animation = 3;
                } else {
                    ((battle_effect_secondary_init_targets_t*)&init)->target[i].fields.result_animation = 0;
                }
            }
        }
    } else if (unit->animate_on_miss_flag == 0) {
        init.target_count = 1;
        init.target.block.values[0] = 1;
        init.target.block.values[1] = unit->target_new_x;
        init.target.block.values[3] = unit->target_new_y;
        init.target.block.values[2] = unit->target_new_map_level;
    }
    spectators = 0;
    for (i = 0; i < 0x10; i++) {
        if (i != unit->unit_id) {
            spectator = battle_unit_get_misc_data_by_misc_id((u16)i);
            if (spectator != 0) {
                stats = spectator->battle_data;
                if (stats != 0) {
                    for (j = 0; j < block->target_count; j++) {
                        if (stats->misc_unit_id == block->target_list[j]) {
                            break;
                        }
                    }
                    if (j < block->target_count) {
                        continue;
                    }
                    init.palette_target_ids[spectators++] = spectator->unit_id;
                }
            }
        }
    }
    init.palette_target_count = spectators;
    init.math_skill_flag = block->control_value_19f;
    init.used_weapon_id = block->used_weapon_id;
    if (block->last_attack_id == 0 || block->reaction_occurred != 0 || g_battle_action_phase != 1
        || (u32)(unit->used_item_or_weapon_id - 0x4A) < 2U || (item = unit->used_item_or_weapon_id) == 0x4C) {
        packed = 0;
    } else {
        if (ability == ABILITY_ID_THROW_BALL) {
            packed = g_battle_ability_animation_data[0x189].charge_animation_set_id;
            packed |= g_main_item_weapon_data[item].element << 16;
        } else {
            packed = g_battle_ability_animation_data[ability].charge_animation_set_id;
            packed |= g_main_ability_range_data[ability].element << 16;
        }
    }
    battle_effect_set_ability_animation(packed, ability, &init);
}

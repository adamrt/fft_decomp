#include "fft/world.h"

/*
 * Combine the stat-detail fields used by equipment comparisons.
 *
 * Multiplier 1 subtracts before from after; -1 adds them. Callers can alias
 * output with before when accumulating equipment changes. Other fields remain
 * unchanged.
 */
void world_item_combine_stat_details(world_item_stat_detail_t* output, world_item_stat_detail_t* before,
    world_item_stat_detail_t* after, s32 multiplier) {
    output->move_bonus = after->move_bonus - multiplier * before->move_bonus;
    output->speed_bonus = after->speed_bonus - multiplier * before->speed_bonus;
    output->jump_bonus = after->jump_bonus - multiplier * before->jump_bonus;
    output->right_weapon_power = after->right_weapon_power - multiplier * before->right_weapon_power;
    output->left_weapon_power = after->left_weapon_power - multiplier * before->left_weapon_power;
    output->right_weapon_evade = after->right_weapon_evade - multiplier * before->right_weapon_evade;
    output->left_weapon_evade = after->left_weapon_evade - multiplier * before->left_weapon_evade;
    output->physical_attack_bonus = after->physical_attack_bonus - multiplier * before->physical_attack_bonus;
    output->physical_class_evade = after->physical_class_evade - multiplier * before->physical_class_evade;
    output->physical_shield_evade = after->physical_shield_evade - multiplier * before->physical_shield_evade;
    output->physical_accessory_evade = after->physical_accessory_evade - multiplier * before->physical_accessory_evade;
    output->magical_attack_bonus = after->magical_attack_bonus - multiplier * before->magical_attack_bonus;
    output->magical_class_evade = after->magical_class_evade - multiplier * before->magical_class_evade;
    output->magical_shield_evade = after->magical_shield_evade - multiplier * before->magical_shield_evade;
    output->magical_accessory_evade = after->magical_accessory_evade - multiplier * before->magical_accessory_evade;
}

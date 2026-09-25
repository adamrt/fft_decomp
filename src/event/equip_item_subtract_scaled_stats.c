#include "fft/event_equip.h"
#include "psx/types.h"

/* Store `base - scale * scaled` for each equipment-stat halfword of an item detail record. */
void equip_item_subtract_scaled_stats(
    world_item_stat_detail_t* out, world_item_stat_detail_t* scaled, world_item_stat_detail_t* base, s32 scale) {
    out->move_bonus = *(u16*)&base->move_bonus - (scale * scaled->move_bonus);
    out->speed_bonus = *(u16*)&base->speed_bonus - (scale * scaled->speed_bonus);
    out->jump_bonus = *(u16*)&base->jump_bonus - (scale * scaled->jump_bonus);
    out->right_weapon_power = *(u16*)&base->right_weapon_power - (scale * scaled->right_weapon_power);
    out->left_weapon_power = *(u16*)&base->left_weapon_power - (scale * scaled->left_weapon_power);
    out->right_weapon_evade = *(u16*)&base->right_weapon_evade - (scale * scaled->right_weapon_evade);
    out->left_weapon_evade = *(u16*)&base->left_weapon_evade - (scale * scaled->left_weapon_evade);
    out->physical_attack_bonus = *(u16*)&base->physical_attack_bonus - (scale * scaled->physical_attack_bonus);
    out->physical_class_evade = *(u16*)&base->physical_class_evade - (scale * scaled->physical_class_evade);
    out->physical_shield_evade = *(u16*)&base->physical_shield_evade - (scale * scaled->physical_shield_evade);
    out->physical_accessory_evade = *(u16*)&base->physical_accessory_evade - (scale * scaled->physical_accessory_evade);
    out->magical_attack_bonus = *(u16*)&base->magical_attack_bonus - (scale * scaled->magical_attack_bonus);
    out->magical_class_evade = *(u16*)&base->magical_class_evade - (scale * scaled->magical_class_evade);
    out->magical_shield_evade = *(u16*)&base->magical_shield_evade - (scale * scaled->magical_shield_evade);
    out->magical_accessory_evade = *(u16*)&base->magical_accessory_evade - (scale * scaled->magical_accessory_evade);
}

#include "fft/battle.h"

void battle_unit_set_attacker_animation_for_shield_block(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target) {
    battle_stats_t* stats;
    s32 kind;
    s32 anim;

    stats = target->battle_data;
    if (stats == 0) {
        return;
    }
    if (g_battle_gfx_spritesheet_data[attacker->spritesheet_id].shp_id >= 2) {
        return;
    }
    kind = stats->action.miss_type;
    anim = 0;
    if (kind >= BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE
        && (kind < BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD || kind == BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD)) {
        switch (attacker->encoded_animation >> 1) {
        case 0x3D:
            anim = 0x5E;
            break;
        case 0x3E:
            anim = 0x5F;
            break;
        case 0x3F:
            anim = 0x60;
            break;
        case 0x40:
            anim = 0x5B;
            break;
        case 0x41:
            anim = 0x5C;
            break;
        case 0x42:
            anim = 0x5D;
            break;
        case 0x4D:
            anim = 0x61;
            break;
        case 0x4E:
            anim = 0x62;
            break;
        case 0x4F:
            anim = 0x63;
            break;
        }
    }
    if (anim != 0) {
        /* The target reads the u16 facing field signed (lh). */
        battle_unit_store_animation_facing_movement_data(anim, (s16)attacker->facing, attacker);
    }
}

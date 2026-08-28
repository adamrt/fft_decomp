#include "fft/battle.h"

void battle_unit_decide_facing_direction(battle_unit_misc_data_t* unit, s32 facing) {
    s16* cur = (s16*)&unit->facing;

    switch (facing) {
    case 0x800:
        if (*cur == facing) {
            return;
        }
        *cur = 0x800;
        break;
    case 0x000:
        if (*cur == 0) {
            return;
        }
        *cur = 0;
        break;
    case 0x400:
        if (*cur == facing) {
            return;
        }
        *cur = 0x400;
        break;
    case 0xC00:
        if (*cur == facing) {
            return;
        }
        *cur = 0xC00;
        break;
    case 0x200:
    case 0x600:
    case 0xA00:
    default:
        return;
    }
    battle_unit_set_animation_based_on_status(unit);
}

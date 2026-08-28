#include "fft/battle.h"
#include "psx/types.h"

typedef struct {
    u8 pad0[0x58];
    u8 x58;
    u8 x59;
    u8 x5A;
    u8 x5B;
    u8 x5C;
} target_data_t;

void battle_formula_apply_evasion_changes_due_to_statuses(void) {
    target_data_t* target;
    u8 flag;
    u8* base_hit;

    target = (target_data_t*)g_battle_action_target;
    flag = (target->x5C & 0x14) != 0;
    if (target->x5B & 2) {
        flag = 1;
    }
    if (target->x59 & 0x10) {
        flag = 1;
    }
    if (target->x58 & 9) {
        flag = 1;
    }
    if (flag) {
        main_util_clear_byte_data(&g_current_ability.accessory_evade, 4);
    }
    if (((target_data_t*)g_battle_action_target)->x58 & 2) {
        base_hit = &g_current_ability.base_hit;
        *base_hit >>= 1;
    }
}

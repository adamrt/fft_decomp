#include "fft/battle.h"

void main_unit_reset_battle_state(battle_stats_t* unit) {
    unit->auto_battle_setting = 0;
    unit->auto_battle_target = 0;
    unit->ct = 0;
    unit->has_turn = 0;
    unit->ability_outcome = 0;
    unit->charged_ability_ct = 0xff;
    unit->mount_info = 0;
    unit->ko_count = 0;
}

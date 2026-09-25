#include "fft/battle.h"
#include "fft/require.h"
#include "psx/types.h"

void require_panel_copy_battle_stats_to_gauges(battle_stats_t* unit, battle_menu_status_panel_gauges_t* output) {
    u16 maximum_hp;
    u16 maximum_mp;
    u8 ct;

    output->level = unit->level;
    output->team_state = 0;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        output->team_state = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        output->team_state = 2;
    }
    if (unit->auto_battle_setting != 0) {
        output->team_state = 3;
    }
    output->experience = unit->experience;
    maximum_hp = unit->max_hp;
    output->max_hp = maximum_hp;
    if (maximum_hp == 0) {
        output->max_hp = maximum_hp + 1;
    }
    output->hp = unit->hp;
    output->_0e = 0;
    maximum_mp = unit->max_mp;
    output->max_mp = maximum_mp;
    if (maximum_mp == 0) {
        output->max_mp = maximum_mp + 1;
    }
    output->mp = unit->mp;
    output->_14 = 0;
    output->max_ct = 100;
    ct = unit->ct;
    output->unit_index = 0;
    output->_04 = 0;
    output->_06 = 0;
    output->ct = ct;
}

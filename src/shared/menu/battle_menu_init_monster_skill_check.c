#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Monster Skill check: the number of the four tiles adjacent to the unit that
 * hold a same-team Monster Skill supporter at the unit's total height. Returns 0
 * for an empty slot. */
s32 battle_menu_init_monster_skill_check(const unit_stats_t* unit) {
    battle_unit_height_data_t height_data;
    s32 team;
    s32 x;
    s32 y;
    s32 height;
    s32 sum;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    battle_unit_calculate_height_data(&height_data, unit->battle_id);
    team = unit->team_flags;
    x = unit->x;
    y = unit->position.y;
    height = height_data.total_height;
    sum = battle_menu_has_monster_skill_supporter_at_tile(team, x - 1, y, height)
        + battle_menu_has_monster_skill_supporter_at_tile(team, x + 1, y, height);
    sum += battle_menu_has_monster_skill_supporter_at_tile(team, x, y - 1, height);
    return sum + battle_menu_has_monster_skill_supporter_at_tile(team, x, y + 1, height);
}

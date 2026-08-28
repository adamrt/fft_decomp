#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/*
 * Collect the monster-skill ability ids a unit can use into `out`, with one
 * range/charge flag byte per accepted id in `flags_out`.
 *
 * The target keeps two live copies of the ability id: `id` in $a0 feeds both
 * sign extensions and the `addiu -1` range check, while `ability` in $a1
 * carries the value the `sh` store writes and is shared with the berserk arm's
 * Frog Attack.
 */
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out) {
    battle_stats_t* unit;
    s32 count;
    s32 berserk;
    s32 i;
    s16 id;
    s16 ability;
    u8 flags;

    count = 3;
    berserk = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) {
        berserk = 1;
    }
    if (g_main_action_menu_types_by_skillset[skillset] != ACTION_MENU_TYPE_MONSTER) {
        *out = -1;
        return 0;
    }
    if (berserk != 0) {
        ability = ABILITY_ID_FROG_ATTACK;
        *out = ability;
        out++;
    }
    if (battle_menu_init_monster_skill_check(unit) != 0) {
        count = MONSTER_SKILLSET_ABILITY_COUNT;
    }
    for (i = 0; i < count; i++) {
        id = main_ability_get_id_from_skillset(skillset, i);
        ability = id;
        if ((berserk == 0 || id == ABILITY_ID_BLACK_MAGIC_FROG) && id != 0 && (u16)(id - 1) < 0x16F) {
            flags = g_main_ability_range_data[id].flags_3;
            *flags_out++ = (flags >> 7) | ((flags & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            *out = ability;
            out++;
        }
    }
}

#include "fft/ability_flags.h"
#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/status.h"
#include "psx/types.h"

/* Builds a unit's 0xff-terminated action-menu skillset list and loads each
 * entry's abilities.
 *
 * `menu_types` receives each entry's low-nibble menu type and `empty` is set
 * to 1 where battle_menu_load_skillset returned 0. A monster skillset in either
 * slot yields a single unloaded primary entry and returns 1; a unit that cannot
 * act or is carrying a rider gets an empty list. Returns -1 when the unit does
 * not exist, otherwise 0.
 *
 * The walk rewinds `entries` from `list` rather than reading `list` in the
 * loop: that keeps `entries` as CSE's canonical copy, so the list stores use
 * the incremented pointer instead of offsets from the saved base. `supports`
 * is declared first so it wins the allocation tie with `primary`. */
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types) {
    battle_stats_t* unit;
    u8* list;
    u8 supports;
    u8 primary;
    u8 secondary;
    s32 i;
    u8 id;
    u8 menu_type;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return -1;
    }
    primary = unit->primary_skillset;
    secondary = unit->secondary_skillset;
    list = entries;
    if (primary >= SKILLSET_ID_MONSTER_FIRST || secondary >= SKILLSET_ID_MONSTER_FIRST) {
        entries[0] = primary;
        empty[0] = 0;
        menu_types[0] = g_main_action_menu_types_by_skillset[primary];
        entries[1] = 0xff;
        return 1;
    }
    supports = unit->support_abilities[3];
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FROG)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) {
        supports = 0;
    }
    if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DONT_ACT)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT))
        || (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
        *entries = 0xff;
        return 0;
    }
    *entries++ = SKILLSET_ID_ATTACK;
    if (primary != 0 && primary != SKILLSET_ID_MIMIC) {
        *entries++ = primary;
    }
    if (secondary != 0) {
        *entries++ = secondary;
    }
    if (supports & BATTLE_SUPPORT_SET_4_DEFEND) {
        *entries++ = SKILLSET_ID_DEFEND;
    }
    if (supports & BATTLE_SUPPORT_SET_4_EQUIP_CHANGE) {
        *entries++ = SKILLSET_ID_EQUIP_CHANGE;
    }
    *entries = 0xff;
    entries = list;
    for (i = 0; i < 6; i++) {
        id = entries[i];
        if (id == 0xff) {
            break;
        }
        menu_type = g_main_action_menu_types_by_skillset[id] & 0xf;
        menu_types[i] = menu_type;
        if (battle_menu_load_skillset(unit_id, id, menu_type) == 0) {
            empty[i] = 1;
        } else {
            empty[i] = 0;
        }
    }
    return 0;
}

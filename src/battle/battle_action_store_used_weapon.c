#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * View of battle_stats_t from 0x16e:
 * last_skillset_id (0x16f), last_ability_id (0x170) and the low byte of
 * used_item_or_equipment (0x176).
 */
typedef struct battle_action_used_weapon_context {
    /* 0x0 */ u8 action_actor_id;
    /* 0x1 */ u8 last_skillset_id;
    /* 0x2 */ s16 last_ability_id;
    /* 0x4 */ u8 _pad04[4];
    /* 0x8 */ u8 used_item_id;
} battle_action_used_weapon_context_t;

/* Pick the weapon the current strike uses. */
void battle_action_store_used_weapon(battle_action_used_weapon_context_t* action) {
    u8 weapon;
    s16 ability_id;

    if (g_current_ability.strike_counter == 0) {
        weapon = g_current_ability.primary_weapon_id;
    } else {
        weapon = g_current_ability.secondary_weapon_id;
    }
    switch (g_main_action_menu_types_by_skillset[action->last_skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_ARITHMETICKS:
    case ACTION_MENU_TYPE_ELEMENTS:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action->last_ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            if ((g_main_ability_range_data[ability_id].flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))
                == 0) {
                weapon = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        weapon = action->used_item_id;
        break;
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        weapon = ITEM_ID_NOTHING;
        break;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = ITEM_ID_NOTHING;
    }
    g_current_ability.weapon_id = weapon;
}

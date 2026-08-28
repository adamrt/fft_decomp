#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Set XA and YA from the current weapon's item type.
 *
 * Bare hands give
 * XA = PA and YA = PA * Brave / 100 (at least 1); otherwise YA is the weapon
 * power and XA depends on the type (PA; (PA + Speed) / 2; PA * Brave / 100;
 * MA; random PA; (PA + MA) / 2), leaving XA unchanged for other types.
 * The attacker's stats must be u8 locals for the (PA + MA) zero-extension.
 */
void battle_formula_calculate_base_xa(void) {
    u8 type;
    u8 pa;
    u8 ma;
    s16 value;
    u8 brave;

    type = g_main_item_primary_data[g_current_ability.weapon_id].type;
    pa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    ma = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    brave = g_battle_action_attacker->brave;
    if (type == ITEM_TYPE_UNARMED) {
        g_current_ability.xa = pa;
        value = pa * brave / 100;
        g_current_ability.ya = value;
        if (value == 0) {
            g_current_ability.ya = value + 1;
        }
        return;
    }
    g_current_ability.ya = g_current_ability.weapon_data.power;
    if (type == ITEM_TYPE_SWORD || type == ITEM_TYPE_CROSSBOW || type == ITEM_TYPE_ROD || type == ITEM_TYPE_POLEARM) {
        g_current_ability.xa = pa;
    } else if (type == ITEM_TYPE_KNIFE || type == ITEM_TYPE_NINJA_BLADE || type == ITEM_TYPE_BOW
        || type == ITEM_TYPE_THROWING) {
        g_current_ability.xa = (pa + g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED]) >> 1;
    } else if (type == ITEM_TYPE_KNIGHT_SWORD || type == ITEM_TYPE_KATANA) {
        value = pa * brave / 100;
        g_current_ability.xa = value;
        if (value == 0) {
            g_current_ability.xa = value + 1;
        }
    } else if (type == ITEM_TYPE_STAFF || type == ITEM_TYPE_POLE) {
        g_current_ability.xa = ma;
    } else if (type == ITEM_TYPE_AXE || type == ITEM_TYPE_FLAIL || type == ITEM_TYPE_BAG) {
        g_current_ability.xa = pa * battle_formula_get_random_0_7fff() / 0x8000 + 1;
    } else if (type == ITEM_TYPE_INSTRUMENT || type == ITEM_TYPE_BOOK || type == ITEM_TYPE_CLOTH) {
        g_current_ability.xa = (pa + ma) >> 1;
    }
}

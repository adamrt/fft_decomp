#include "fft/event_require.h"
#include "fft/script.h"
#include "psx/types.h"

/*
 * Builds the war-trophy entry list and pays the trophies out.
 *
 * The bonus money (when nonzero) is the first entry, followed by one entry
 * per trophy item found by battle_unit_find_war_trophies_and_bonus_money.
 * Money entries are added to the war funds. An item entry adds one to the
 * inventory, unless 99 are already held or equipped: the item is then sold
 * for a quarter of its price, rounded toward zero.
 */
void require_reward_collect_war_trophies_and_bonus_money(void) {
    s32 i;
    s32 item_id;
    s32 quantity;
    s32 price;
    s32 adj;
    s32 funds;
    s32 item_count;
    item_data_t* item;
    u8* qp;
    u32 pad[4];
    s32* iv;
    s32* it;

    g_require_reward_war_trophy_entry_count = 0;
    battle_unit_find_war_trophies_and_bonus_money(&g_require_reward_war_trophy_search_result);

    {
        s32 j;
        s32 next;
        iv = g_require_reward_war_trophy_entry_values;
        it = g_require_reward_war_trophy_entry_types;
        j = g_require_reward_war_trophy_entry_count;
        iv[j] = g_require_reward_war_trophy_search_result.bonus_money;
        it[j] = REQUIRE_WAR_TROPHY_ENTRY_BONUS_MONEY;
        if (iv[j] != 0) {
            next = j + 1;
            g_require_reward_war_trophy_entry_count = next;
        }
    }

    i = 0;
    item_count = g_require_reward_war_trophy_item_count;
    if (item_count > 0) {
        s32* tb;
        s32* vb = iv;
        u8* cp;
        s32 entry;
        s32 loop_count;
        tb = it;
        cp = &g_require_reward_war_trophy_search_result.item_count;
        do {
            entry = g_require_reward_war_trophy_entry_count;
            item_id = g_require_reward_war_trophy_search_result.item_ids[i];
            i++;
            vb[entry] = item_id;
            tb[entry] = REQUIRE_WAR_TROPHY_ENTRY_ITEM;
            loop_count = *cp;
            g_require_reward_war_trophy_entry_count = entry + 1;
        } while (i < loop_count);
    }

    i = 0;
    if (g_require_reward_war_trophy_entry_count > 0) {
        s32* vp = g_require_reward_war_trophy_entry_values;
        s32* tp = g_require_reward_war_trophy_entry_types;
        do {
            if (*tp == REQUIRE_WAR_TROPHY_ENTRY_ITEM) {
                item_id = *vp;
                quantity = main_item_get_total_equipment_quantity(item_id, 1);
                if (quantity == 99) {
                    item = main_item_get_data_pointer(*vp);
                    price = item->price;
                    if (price < 0) {
                        adj = price + 3;
                    } else {
                        adj = price;
                    }
                    price = adj >> 2;
                    funds = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
                    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + price);
                } else {
                    s32 id = *vp;
                    qp = &g_main_item_quantities[id];
                    (*qp)++;
                }
            } else {
                funds = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
                script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, funds + *vp);
            }
            vp++;
            tp++;
        } while (++i < g_require_reward_war_trophy_entry_count);
    }
}

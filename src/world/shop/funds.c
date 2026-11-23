/* Shop transactions adjust party funds and cash out inventory above the retail holding limit. */

#include "fft/game/item.h"
#include "fft/party/shop.h"
#include "fft/script/script_variable.h"
#include "psx/types.h"

/* Add `delta` to the war funds, clamped to 0..99,999,999, and return the new
 * amount. */
s32 shop_adjust_gil(s32 delta) {
    s32 gil = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS) + delta;

    if (gil < 0) {
        gil = 0;
    }
    if (gil > EVENT_WAR_FUNDS_MAX) {
        gil = EVENT_WAR_FUNDS_MAX;
    }
    script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, gil);
    return gil;
}

/* Convert inventory copies above the owned-item limit into gil.
 *
 * Equipped copies count toward 99; only inventory quantities are reduced.
 */
void shop_cash_out_excess_items(void) {
    s32 item_id = 0;
    s32 excess;
    s32 price;
    u8* quantity = g_item_quantities;

    for (; item_id < ITEM_ID_END - 1; item_id++) {
        excess = item_get_total_count(item_id);
        if (excess >= 100) {
            price = shop_get_item_half_price(item_id);
            excess -= 99;
            shop_adjust_gil(price * excess);
            *quantity -= excess;
        }
        quantity++;
    }
}

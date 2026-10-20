/* Shop transactions adjust party funds and cash out inventory above the retail holding limit. */

#include "fft/item.h"
#include "fft/menu_script.h"
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "psx/types.h"

/* The dismiss prompt keeps its eligibility result until an answer queues a sound. */
enum {
    FORMATION_DISMISS_REFUSED = 0,
    FORMATION_DISMISS_CONFIRM = 1,
    FORMATION_DISMISS_PROPOSITION_MESSAGE = 2,
    FORMATION_DISMISS_SOUND_QUEUED = 100
};

/* Add `delta` to the war funds, clamped to 0..99,999,999, and return the new
 * amount. */
s32 shop_gil_adjust(s32 delta) {
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
        excess = item_total_count_get(item_id);
        if (excess >= 100) {
            price = shop_item_half_price_get(item_id);
            excess -= 99;
            shop_gil_adjust(price * excess);
            *quantity -= excess;
        }
        quantity++;
    }
}

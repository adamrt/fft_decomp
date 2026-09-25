#include "fft/world.h"

/* Convert inventory copies above the owned-item limit into gil.
 *
 * Equipped copies count toward 99; only inventory quantities are reduced.
 */
void world_item_cash_out_excess_inventory(void) {
    s32 item_id;
    s32 excess;
    s32 price;
    u8* quantity;

    item_id = 0;
    quantity = g_main_item_quantities;
    for (; item_id < 253; item_id++) {
        excess = world_item_count_owned_and_equipped((s16)item_id);
        if (excess >= 100) {
            price = world_item_get_half_price((s16)item_id);
            excess -= 99;
            world_shop_obtain_gil(price * excess);
            *quantity -= excess;
        }
        quantity++;
    }
}

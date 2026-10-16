/* Reconciling an item category list with what the party owns. */

#include "fft/data.h"
#include "fft/item.h"
#include "psx/types.h"

/* Reconcile one sorted item category list (0 weapons and shields, then
 * headgear, body armor, accessories and other: the menu category minus one)
 * with what the party owns: a newly owned item is put first, an item neither
 * owned nor equipped is removed, and the others keep their order. */
void item_rebuild_category_list(s32 category) {
    s32 i;
    s16 item;
    s32 type;
    s32 cat;

    for (i = 1; i < ITEM_ID_END; i++) {
        item = i;
        type = item_get_menu_category(item);
        cat = 0;
        if (type != ITEM_MENU_CATEGORY_WEAPON) {
            cat = type - 1;
        }
        if (category == cat) {
            if (item_get_total_count_variant(item) != 0) {
                if (item_is_in_category_list(i, category) == 0) {
                    item_prepend_to_category_list(i, category);
                }
            } else {
                item_remove_from_category_list(i, category);
            }
        }
    }
}

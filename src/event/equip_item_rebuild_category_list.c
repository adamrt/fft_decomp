#include "fft/event_equip.h"

void equip_item_rebuild_category_list(s32 category) {
    s32 i;
    s16 item;
    s32 type;
    s32 cat;

    for (i = 1; i < 0xFE; i++) {
        item = i;
        type = equip_item_get_category(item);
        cat = 0;
        if (type != 0) {
            cat = type - 1;
        }
        if (category == cat) {
            if (equip_item_get_total_count(item) != 0) {
                if (equip_item_is_in_category_list(i, category) == 0) {
                    equip_item_prepend_to_category_list(i, category);
                }
            } else {
                equip_item_remove_from_category_list(i, category);
            }
        }
    }
}

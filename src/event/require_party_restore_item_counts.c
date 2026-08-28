#include "fft/main_runtime.h"
#include "fft/main_unit.h"

s32 require_party_restore_item_counts(s32 party_index) {
    party_data_t* party = main_party_get_data_pointer(party_index);
    s32 index = 0;

    do {
        u8 item_id = party->equipment[index];

        index++;
        if ((item_id != 0) && (item_id != ITEM_ID_NONE)) {
            g_main_item_quantities[item_id]++;
        }
    } while (index < 7);
    return 0;
}

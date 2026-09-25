#include "fft/battle.h"
#include "psx/types.h"

/* The `j = 0;` before `none = 0xFF;` is load-bearing: it puts the induction
 * variable's initialisation first in the if-body so the branch's delay slot
 * takes `move a0,zero` rather than `li a2,0xff`. */
void battle_dismiss_unit_event_instruction(s32 unit_id) {
    party_data_t* party;
    s32 i;
    s32 j;
    s32 none;
    u8 item_id;

    for (i = 1; i < 0x14; i++) {
        party = main_party_get_data_pointer(i);
        if (party->party_id != 0xFF && party->sprite_set == unit_id) {
            break;
        }
    }
    if (i != 0x14) {
        j = 0;
        none = 0xFF;
        for (; j < 7; j++) {
            item_id = party->equipment[j];
            if (item_id == 0 || item_id == none) {
                continue;
            }
            g_main_item_quantities[item_id]++;
        }
        main_party_remove_unit(i);
    }
}

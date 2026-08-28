#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

void world_script_dismiss_unit_event_instruction(s32 sprite_set) {
    party_data_t* party_data;
    s32 party_index = 1;
    s32 empty_party_id = 0xff;
    s32 empty_item;
    s32 slot;
    u8 item;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_data = main_party_get_data_pointer(party_index);
        if (party_data->party_id != empty_party_id && party_data->sprite_set == sprite_set) {
            break;
        }
    }

    if (party_index != PARTY_ROSTER_SLOT_COUNT) {
        slot = 0;
        empty_item = ITEM_ID_NONE;
        for (; slot < 7; slot++) {
            item = party_data->equipment[slot];
            if (item != ITEM_ID_NOTHING && item != empty_item) {
                g_main_item_quantities[item]++;
            }
        }
        main_party_remove_unit(party_index);
    }
}

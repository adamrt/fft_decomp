#include "fft/main.h"

s32 main_party_find_free_slot(s32 save_formation, u8* palette) {
    party_data_t* party_data[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 start_index;
    s32 limit;

    *palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_data[party_index] = main_party_get_data_pointer(party_index);
    }

    if (save_formation != 0) {
        start_index = PARTY_GUEST_SLOT_FIRST;
        limit = PARTY_ROSTER_SLOT_COUNT;
    } else {
        start_index = 0;
        limit = PARTY_GUEST_SLOT_FIRST;
    }

    party_index = start_index;

    while (party_index < limit) {
        if (party_data[party_index]->party_id == PARTY_ID_NONE) {
            return party_index;
        }
        party_index++;
    }

    return -1;
}

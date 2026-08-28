#include "fft/main_unit.h"

s32 main_party_generate_unit(s32 unit_type) {
    party_data_t* party_data[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_data[party_index] = main_party_get_data_pointer(party_index);
    }

    for (party_index = 0; party_index < 16; party_index++) {
        if (party_data[party_index]->party_id == PARTY_ID_NONE) {
            party_data[party_index]->party_id = party_index;
            party_data[party_index]->palette = palette;
            main_unit_generate_out_of_battle(party_data[party_index], unit_type);
            return party_index;
        }
    }

    return -1;
}

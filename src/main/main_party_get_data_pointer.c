#include "fft/main_unit.h"

party_data_t* main_party_get_data_pointer(s32 party_index) {
    party_data_t* party_data;

    if (party_index < PARTY_ROSTER_SLOT_COUNT) {
        party_data = &g_main_party_data[party_index];
    } else {
        party_data = 0;
    }
    return party_data;
}

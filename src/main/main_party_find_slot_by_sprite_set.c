#include "fft/main.h"

s32 main_party_find_slot_by_sprite_set(s32 sprite_set) {
    s32 party_index = 0;
    s32 empty_party_id = PARTY_ID_NONE;
    party_data_t* party_data = g_main_party_data;

    while (party_index < PARTY_ROSTER_SLOT_COUNT) {
        if (party_data->party_id != empty_party_id && party_data->sprite_set == sprite_set) {
            return party_index;
        }
        party_index++;
        party_data++;
    }

    return -1;
}

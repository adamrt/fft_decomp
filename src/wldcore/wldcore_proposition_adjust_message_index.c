#include "fft/wldcore.h"

/* Bravery/faith bands used to index the 3x3 table in proposition record 2:
 * < 0x24 -> 0, < 0x42 -> 1, < 0x65 -> 2 (a value of 0x65+ leaves the band
 * uninitialized, as in the target). */
/* Target 0x800793f0. */
s32 wldcore_proposition_adjust_message_index(s32 base, s32 party_index) {
    s32 faith_band;
    s32 bravery_band;
    s32 is_not_male;
    party_data_t* party;

    party = wldcore_get_party_data_pointer(party_index);
    if (party->bravery < 0x65) {
        bravery_band = 2;
    }
    if (party->bravery < 0x42) {
        bravery_band = 1;
    }
    if (party->bravery < 0x24) {
        bravery_band = 0;
    }
    if (party->faith < 0x65) {
        faith_band = 2;
    }
    if (party->faith < 0x42) {
        faith_band = 1;
    }
    if (party->faith < 0x24) {
        faith_band = 0;
    }
    is_not_male = party->gender_flags & UNIT_FLAG_MALE;
    is_not_male = (u32)is_not_male < 1;
    return base + wldcore_proposition_get_data_pointer(2)->message_adjustments[faith_band * 3 + bravery_band]
        + is_not_male * 3;
}

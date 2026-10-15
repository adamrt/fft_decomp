#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

s32 main_party_find_slot_by_sprite_set(s32 sprite_set) {
    s32 party_index = 0;
    s32 empty_party_id = PARTY_ID_NONE;
    party_unit_t* party = g_main_party_units;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++, party++) {
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            return party_index;
        }
    }

    return -1;
}

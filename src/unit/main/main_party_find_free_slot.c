#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Return the first empty roster slot, or -1 when the searched range is full.
 *
 * A nonzero save_formation searches the four guest slots (16..19), zero the
 * sixteen ordinary slots. *palette is always set to 0. */
s32 main_party_find_free_slot(s32 save_formation, u8* palette) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 start_index;
    s32 limit;

    *palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = main_party_get_unit(party_index);
    }

    if (save_formation != 0) {
        start_index = PARTY_GUEST_SLOT_FIRST;
        limit = PARTY_ROSTER_SLOT_COUNT;
    } else {
        start_index = 0;
        limit = PARTY_MEMBER_SLOT_COUNT;
    }

    for (party_index = start_index; party_index < limit; party_index++) {
        if (party_units[party_index]->party_id == PARTY_ID_NONE) {
            return party_index;
        }
    }

    return -1;
}

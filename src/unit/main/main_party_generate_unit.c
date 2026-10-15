#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Generate a new unit of unit_type (MAIN_UNIT_TYPE_*) in the first empty ordinary
 * roster slot (0..15), with palette 0. Returns the slot, or -1 when all sixteen
 * are taken. */
s32 main_party_generate_unit(main_unit_generation_type_e unit_type) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = main_party_get_unit(party_index);
    }

    for (party_index = 0; party_index < PARTY_MEMBER_SLOT_COUNT; party_index++) {
        if (party_units[party_index]->party_id == PARTY_ID_NONE) {
            party_units[party_index]->party_id = party_index;
            party_units[party_index]->palette = palette;
            main_unit_generate_out_of_battle(party_units[party_index], unit_type);
            return party_index;
        }
    }

    return -1;
}

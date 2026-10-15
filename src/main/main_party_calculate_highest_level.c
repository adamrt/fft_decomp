#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Return the highest level in the roster, capped at 99, and store it in
 * g_main_party_highest_level. */
u8 main_party_calculate_highest_level(void) {
    u8 highest;
    s32 i;
    party_unit_t* unit;

    highest = 0;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE && highest < unit->level) {
            highest = unit->level;
        }
    }
    if (highest > UNIT_LEVEL_MAX) {
        highest = UNIT_LEVEL_MAX;
    }
    g_main_party_highest_level = highest;
    return highest;
}

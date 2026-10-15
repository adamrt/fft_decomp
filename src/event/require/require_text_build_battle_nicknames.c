#include "fft/event_require.h"
#include "psx/types.h"

/* Rebuild the roster name text section from the battle units: one 0xFE
 * terminated name per battle unit slot, with a terminator appended to a name
 * that fills all 16 bytes. */
void require_text_build_battle_nicknames(void) {
    u8* output;
    s32 unit_id;

    output = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = battle_unit_get_stats_from_battle_id(unit_id);
        s32 length = 0;

        do {
            u8 value = unit->name[length];
            *output = value;
            if ((value & 0xfe) != 0xfe) {
                output++;
                if (length == 15) {
                    *output++ = 0xfe;
                }
            } else {
                output++;
                break;
            }
            length++;
        } while (length < 16);
    }
}

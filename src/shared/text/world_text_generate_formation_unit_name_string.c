#include "fft/world.h"

/* Rebuild the roster name text section from the party data: one 0xFE
 * terminated name per roster slot, empty for a vacant slot, with a terminator
 * appended to a name that fills all 16 bytes. Bumps the name string
 * revision. */
void world_text_generate_formation_unit_name_string(void) {
    u8* out;
    party_unit_t* unit;
    s32 i;
    s32 j;
    u8 character;

    out = g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id == PARTY_ID_NONE) {
            *out = 0xFE;
            out++;
        } else {
            for (j = 0; j < 0x10; j++) {
                character = unit->name[j];
                *out = character;
                if ((character & 0xFE) == 0xFE) {
                    out++;
                    break;
                }
                out++;
                if (j == 0xF) {
                    *out = 0xFE;
                    out++;
                }
            }
        }
    }
    g_world_formation_name_string_revision = g_world_formation_name_string_revision + 1;
}

#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"

void world_text_generate_formation_unit_name_string(void) {
    u8* out;
    party_data_t* unit;
    s32 i;
    s32 j;
    u8 character;

    out = g_world_text_roster_unit_names;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_data_pointer(i);
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

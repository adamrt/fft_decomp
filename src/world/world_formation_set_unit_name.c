#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"

/*
 * Set a unit's name in both the formation and persistent party records.
 *
 * Copy through the first 0xfe byte, or stop after 16 bytes without appending
 * a terminator. The remaining name bytes and name ID are left unchanged.
 * Rebuild the menu's name strings after updating both records.
 */
void world_formation_set_unit_name(s32 formation_index, const u8* name) {
    party_data_t* party;
    s32 i;
    u8 character;

    party = main_party_get_data_pointer(g_world_formation_unit_pointers[(s16)formation_index]->roster_slot);
    for (i = 0; i < 16; i++) {
        character = *name;
        g_world_formation_unit_pointers[(s16)formation_index]->name[i] = character;
        party->name[i] = character;
        if (*name == 0xfe) {
            break;
        }
        name++;
    }
    world_text_generate_formation_unit_name_string();
}

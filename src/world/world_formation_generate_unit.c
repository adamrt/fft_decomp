#include "fft/data.h"
#include "fft/main_unit.h"
#include "fft/world.h"

/*
 * Generate a party unit and return its rebuilt formation-list index.
 *
 * Temporarily substitute a menu name-buffer pointer for generation, then clear
 * the new unit's secondary skillset and reaction, support, and movement slots.
 * Return -1 if generation fails; otherwise rebuild the formation workspace.
 */
s32 world_formation_generate_unit(s32 unit_type) {
    u8** name_buffer = &g_world_text_roster_unit_names;
    u8* saved_names;
    s32 roster_slot;
    party_data_t* party;

    saved_names = *name_buffer;
    *name_buffer = g_world_text_unit_names;
    roster_slot = main_party_generate_unit((s16)unit_type);
    *name_buffer = saved_names;
    if (roster_slot < 0) {
        return -1;
    }
    party = main_party_get_data_pointer(roster_slot);
    party->secondary_skillset = 0;
    /* Byte stores are required for the exact match; halfword assignments emit SH. */
    party->reaction_ability[0] = 0;
    party->reaction_ability[1] = 0;
    party->support_ability[0] = 0;
    party->support_ability[1] = 0;
    party->movement_ability[0] = 0;
    party->movement_ability[1] = 0;
    world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
    return world_formation_find_index_by_roster_slot((s16)roster_slot);
}

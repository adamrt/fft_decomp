/* Dismissing units, generating units and setting unit names. */

#include "fft/data_item.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_text.h"
#include "fft/unit.h"
#include "fft/world_formation.h"
#include "fft/world_text.h"
#include "psx/types.h"

/* Ramza's sprite sets and guest roster slots cannot be
 * dismissed. */
s32 world_formation_can_dismiss_unit(s16 formation_idx) {
    world_unit_t* unit = g_world_unit_pointers[formation_idx];
    s32 sprite_set = unit->sprite_set;
    s32 roster_slot = unit->identity.roster_slot;
    if (sprite_set <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
        return 0;
    return roster_slot < PARTY_GUEST_SLOT_FIRST;
}

/* Remove a formation unit from the roster, unequipping its five slots first
 * so the items return to the inventory. Does nothing when
 * world_formation_can_dismiss_unit refuses. */
void world_formation_dismiss_unit(s16 formation_idx) {
    s32 i;

    if (world_formation_can_dismiss_unit(formation_idx) != 0) {
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            world_formation_equip_item_to_unit_slot(formation_idx, i, ITEM_ID_NOTHING);
        }
        main_party_remove_unit(g_world_unit_pointers[formation_idx]->identity.roster_slot);
    }
}

/*
 * Generate a party unit and return its rebuilt formation-list index.
 *
 * Temporarily substitute a menu name-buffer pointer for generation, then clear
 * the new unit's secondary skillset and reaction, support, and movement slots.
 * Return -1 if generation fails; otherwise rebuild the formation workspace.
 */
s32 world_formation_generate_unit(s16 unit_type) {
    u8** name_buffer = &g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    u8* saved_names;
    s32 roster_slot;
    party_unit_t* party;

    saved_names = *name_buffer;
    *name_buffer = g_text_section_pointers[TEXT_SECTION_UNIT_NAMES];
    roster_slot = main_party_generate_unit(unit_type);
    *name_buffer = saved_names;
    if (roster_slot < 0) {
        return -1;
    }
    party = main_party_get_unit(roster_slot);
    party->secondary_skillset = 0;
    /* Byte stores are required for the exact match; 16-bit assignments emit SH. */
    party->reaction_ability[0] = 0;
    party->reaction_ability[1] = 0;
    party->support_ability[0] = 0;
    party->support_ability[1] = 0;
    party->movement_ability[0] = 0;
    party->movement_ability[1] = 0;
    world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
    return world_formation_find_index_by_roster_slot(roster_slot);
}

/*
 * Set a unit's name in both the formation and persistent party records.
 *
 * Copy through the first 0xfe byte, or stop after 16 bytes without appending
 * a terminator. The remaining name bytes and name ID are left unchanged.
 * Rebuild the menu's name strings after updating both records.
 */
void world_formation_set_unit_name(s16 formation_index, const u8* name) {
    party_unit_t* party = main_party_get_unit(g_world_unit_pointers[formation_index]->identity.roster_slot);
    s32 i;
    u8 character;

    for (i = 0; i < UNIT_NAME_SIZE; i++) {
        character = *name;
        g_world_unit_pointers[formation_index]->name[i] = character;
        party->name[i] = character;
        if (*name == 0xfe) {
            break;
        }
        name++;
    }
    world_text_build_roster_name_list();
}

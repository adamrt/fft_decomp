/* Dismissing units, generating units and setting unit names. */

#include "fft/formation.h"
#include "fft/formation_item.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/party.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* Return whether a formation unit can be dismissed: not Ramza (any chapter's sprite set) and
 * not a guest roster slot. */
s32 formation_can_dismiss(s16 formation_idx) {
    formation_unit_t* unit = g_formation_unit_pointers[formation_idx];
    s32 sprite_set = unit->sprite_set;
    s32 roster_slot = unit->identity.roster_slot;

    if (sprite_set <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
        return 0;
    return roster_slot < PARTY_GUEST_SLOT_FIRST;
}

/* Remove a formation unit from the roster, unequipping its five slots first
 * so the items return to the inventory. Does nothing when
 * formation_can_dismiss refuses. */
void formation_dismiss(s16 formation_idx) {
    s32 i;

    if (formation_can_dismiss(formation_idx) != 0) {
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            formation_item_equip(formation_idx, i, ITEM_ID_NOTHING);
        }
        party_remove_unit(g_formation_unit_pointers[formation_idx]->identity.roster_slot);
    }
}

/*
 * Generate a party unit and return its rebuilt formation-list index.
 *
 * Temporarily substitute a menu name-buffer pointer for generation, then clear
 * the new unit's secondary skillset and reaction, support, and movement slots.
 * Return -1 if generation fails; otherwise rebuild the formation workspace.
 */
s32 formation_generate_unit(s16 unit_type) {
    u8** name_buffer = &g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    u8* saved_names;
    s32 roster_slot;
    party_unit_t* party;

    saved_names = *name_buffer;
    *name_buffer = g_text_section_pointers[TEXT_SECTION_UNIT_NAMES];
    roster_slot = party_generate_unit(unit_type);
    *name_buffer = saved_names;
    if (roster_slot < 0) {
        return -1;
    }
    party = party_get_unit(roster_slot);
    party->secondary_skillset = 0;
    /* Byte stores are required for the exact match; 16-bit assignments emit SH. */
    party->reaction_ability.low = 0;
    party->reaction_ability.high = 0;
    party->support_ability.low = 0;
    party->support_ability.high = 0;
    party->movement_ability.low = 0;
    party->movement_ability.high = 0;
    formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
    return formation_find_by_roster_slot(roster_slot);
}

/*
 * Set a unit's name in both the formation and persistent party records.
 *
 * Copy through the first 0xfe byte, or stop after 16 bytes without appending
 * a terminator. The remaining name bytes and name ID are left unchanged.
 * Rebuild the menu's name strings after updating both records.
 */
void formation_set_unit_name(s16 formation_index, const u8* name) {
    party_unit_t* party = party_get_unit(g_formation_unit_pointers[formation_index]->identity.roster_slot);
    s32 i;
    u8 character;

    for (i = 0; i < UNIT_NAME_SIZE; i++) {
        character = *name;
        g_formation_unit_pointers[formation_index]->name[i] = character;
        party->name[i] = character;
        if (*name == 0xfe) {
            break;
        }
        name++;
    }
    formation_build_roster_names();
}

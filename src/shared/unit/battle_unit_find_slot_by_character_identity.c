#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Classify a character identity for battle entry: the stats slot when
 * an active unit has it (-3 if it is crystallised or a treasure), -2/-3 for an
 * absent unit whose existence byte is 0/BATTLE_UNIT_EXISTENCE_DISABLED, -4
 * when a party roster entry
 * has it, else -5; -1 for selector identities.
 *
 * The two status tests must be separate `if`s with their own `return -3`: an
 * `||` lets the delay-slot pass steal `li v0,-3` into the first branch.
 */
s32 battle_unit_find_slot_by_character_identity(u32 character_identity) {
    unit_stats_t* unit;
    party_unit_t* party;
    s32 i;
    u8* status;
    u8 identity;

    if (character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return BATTLE_CHARACTER_SLOT_SELECTOR;
    }
    identity = character_identity;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->character_identity == identity) {
            status = unit->status_sets.current;
            if (status[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
            if (status[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
            return i;
        }
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->character_identity == identity
            && unit->existence != BATTLE_UNIT_EXISTENCE_NONE) {
            if (unit->existence == BATTLE_UNIT_EXISTENCE_OFF_FIELD) {
                return BATTLE_CHARACTER_SLOT_OFF_FIELD;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED) {
                return BATTLE_CHARACTER_SLOT_UNAVAILABLE;
            }
        }
    }
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = main_party_get_unit(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set == identity) {
            return BATTLE_CHARACTER_SLOT_IN_PARTY;
        }
    }
    return BATTLE_CHARACTER_SLOT_ABSENT;
}

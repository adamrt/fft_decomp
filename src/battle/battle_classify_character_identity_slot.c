#include "fft/battle.h"
#include "psx/types.h"

/*
 * Classify a character identity for battle entry: the battle_stats slot when
 * an active unit has it (-3 if it is crystallised or a treasure), -2/-3 for an
 * absent unit whose existence byte is 0/BATTLE_UNIT_EXISTENCE_DISABLED, -4
 * when a party roster entry
 * has it, else -5; -1 for selector identities.
 *
 * The two status tests must be separate `if`s with their own `return -3`: an
 * `||` lets the delay-slot pass steal `li v0,-3` into the first branch.
 */
s32 battle_classify_character_identity_slot(u32 character_identity) {
    battle_stats_t* unit;
    party_data_t* party;
    s32 i;
    u8* status;

    if (character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return -1;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &battle_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->character_identity == (u8)character_identity) {
            status = unit->status_sets.current;
            if (status[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL)) {
                return -3;
            }
            if (status[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TREASURE)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
                return -3;
            }
            return i;
        }
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &battle_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE && unit->character_identity == (u8)character_identity
            && unit->existence != 0xff) {
            if (unit->existence == 0) {
                return -2;
            }
            if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED) {
                return -3;
            }
        }
    }
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = main_get_party_data_pointer(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set == (u8)character_identity) {
            return -4;
        }
    }
    return -5;
}

#include "fft/wldcore.h"

/* Reject the MCBrave>= condition when Ramza's Brave is below the operand. */
void wldcore_opcode_map_check_ramza_brave_at_least(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->bravery < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Roster index (0..15) of the first occupied slot holding one of Ramza's
 * identities; 16 when there is none. */
s32 wldcore_get_ramza_roster_index(void) {
    s32 i;
    for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
        party_unit_t* party = wldcore_get_party_unit(i);
        if (party->party_id != PARTY_ID_NONE && party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE
            && party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            return i;
        }
    }
    return i;
}

/* Reject the MCBrave<= condition when Ramza's Brave is above the operand. */
void wldcore_opcode_map_check_ramza_brave_at_most(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->bravery) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith>= condition when Ramza's Faith is below the operand. */
void wldcore_opcode_map_check_ramza_faith_at_least(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (party->faith < val) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

/* Reject the MCFaith<= condition when Ramza's Faith is above the operand. */
void wldcore_opcode_map_check_ramza_faith_at_most(void) {
    party_unit_t* party;
    u16 ip;
    s32 val;

    party = wldcore_get_party_unit(wldcore_get_ramza_roster_index());
    ip = g_wldcore_script_state.ip;
    val = g_wldcore_script_state.data[ip];
    g_wldcore_script_state.ip = ip + 1;
    if (val < party->faith) {
        g_wldcore_script_state.flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}

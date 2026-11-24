/* Event commands that change the party roster, Brave, calendar or a unit display.
 * The paired-unit warp and tile-depth switches support scripted scene changes. */

#include "fft/battle/unit_view.h"
#include "fft/game/battle_unit.h"
#include "fft/game/character_identity.h"
#include "fft/game/item.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/map/map.h"
#include "fft/map/map_unit.h"
#include "fft/script/script.h"
#include "fft/script/script_instruction.h"
#include "fft/script/script_variable.h"
#include "fft/world/calendar.h"
#include "psx/types.h"

static map_tile_coordinates_t s_script_warp_coords;

/* SCRIPT_OPCODE_DISMISS_UNIT (0x7a)
 * DismissUnit: remove the first roster member after slot 0 with this sprite set from
 * the party, returning its equipment to the inventory. Nothing happens if none matches. */
void script_opcode_dismiss_unit(s32 sprite_set) {
    party_unit_t* party;
    s32 party_index = 1;
    s32 empty_party_id = PARTY_ID_NONE;
    s32 empty_item;
    s32 slot;
    u8 item;

    /* Each counter is set apart from its loop, before the comparison value, for the
     * original instruction order. */
    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party = party_unit_get(party_index);
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            break;
        }
    }

    if (party_index != PARTY_ROSTER_SLOT_COUNT) {
        slot = 0;
        empty_item = ITEM_ID_NONE;
        for (; slot < UNIT_EQUIPMENT_SLOT_COUNT; slot++) {
            item = party->equipment[slot];
            if (item != ITEM_ID_NOTHING && item != empty_item) {
                g_item_quantities[item]++;
            }
        }
        party_unit_remove(party_index);
    }
}

/* Persist event units marked to join without opening the formation screen. */
void party_join_event_units(void) {
    s32 out_idx;
    s32 i;
    battle_unit_t* unit;
    u8 first;
    u8 flags;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_by_unit_id(battle_unit_get(i)->unit_id, &out_idx);
        if (out_idx >= 0 && i == out_idx) {
            if (unit->formation_index == UNIT_FORMATION_INDEX_NONE
                || ((first = unit->character_identity) != CHARACTER_IDENTITY_ENTD_NONE
                    && first <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)) {
                flags = unit->unit_flags;
                if (flags & UNIT_FLAG_JOIN_AFTER_EVENT) {
                    party_unit_save(unit, flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }
}

/* CallFunction 2 and 7: stage a 365-day advance in the date-advance variable. The
 * calendar itself moves when that variable is consumed. */
void calendar_request_year_advance(void) {
    script_variable_set(SCRIPT_VAR_DATE_ADVANCE, 365);
}

/* CallFunction 3: raise the Brave of every blue-team unit by 10, capped at UNIT_BRAVE_FAITH_MAX. */
void battle_unit_increase_blue_team_brave_by_10(void) {
    battle_unit_t* unit;
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit = battle_unit_get(unit_id);
        if ((unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= UNIT_BRAVE_FAITH_MAX) {
                unit->brave = UNIT_BRAVE_FAITH_MAX;
            }
        }
    }
}

/* CallFunction 0x0f and 0x10: place the map unit of battle unit id 3 on the tile and
 * facing of battle unit id 1, or that of battle unit id 4 on battle unit id 2, and cancel any
 * rotation in progress. Nothing happens when the warped battle unit id has no runtime
 * unit.
 *
 * The parameter arrives as the CallFunction selector and is reused for the runtime
 * id. */
void script_warp_unit_display_to_paired_unit(s32 map_unit_id) {
    battle_unit_t* unit;
    unit_animation_state_t* state;
    s32 facing;
    s32 battle_unit_id;

    if (map_unit_id == SCRIPT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) {
        unit = battle_unit_get(1);
        battle_unit_id = 3;
    } else {
        unit = battle_unit_get(2);
        battle_unit_id = 4;
    }
    map_unit_id = map_unit_get_id_by_battle_unit_id(battle_unit_id);
    if (map_unit_id != -1) {
        s_script_warp_coords.x = unit->x;
        s_script_warp_coords.layer = unit->position.higher_elevation;
        s_script_warp_coords.y = unit->position.y;
        facing = unit->position.facing << 2;
        state = &g_unit_animation_states[map_unit_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        map_unit_place_in_new_location(map_unit_id, &s_script_warp_coords, unit->position.facing);
        map_unit_set_event_facing(map_unit_id, unit->position.facing << 2);
    }
}

/* Hide the depth number of the selected-tile panel; BATTLE's effect playback sets it for the
 * length of an ability effect. */
void unit_view_hide_tile_depth(void) {
    g_unit_view_depth_display_suppressed = 1;
}

/* Show the selected-tile panel's depth number again (effect playback end). */
void unit_view_show_tile_depth(void) {
    g_unit_view_depth_display_suppressed = 0;
}

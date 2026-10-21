/* Event commands that change the party roster, Brave, calendar or a unit display.
 * The paired-unit warp and tile-depth switches support scripted scene changes. */

#include "fft/battle_map.h"
#include "fft/calendar.h"
#include "fft/item.h"
#include "fft/party.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/types.h"

static battle_map_tile_coordinates_t s_script_warp_coords;
extern u16 g_unit_view_depth_display_suppressed;

/* DismissUnit: remove the first roster member after slot 0 with this sprite set from
 * the party, returning its equipment to the inventory. Nothing happens if none matches. */
void script_opcode_7a_dismiss_unit(s32 sprite_set) {
    party_unit_t* party;
    s32 party_index = 1;
    s32 empty_party_id = PARTY_ID_NONE;
    s32 empty_item;
    s32 slot;
    u8 item;

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
void party_event_units_join(void) {
    s32 out_idx;
    s32 i;
    unit_stats_t* unit;
    u8 first;
    u8 flags;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_entd_find_stats(unit_stats_get(i)->unit_id, &out_idx);
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
void calendar_year_advance(void) {
    script_variable_set(EVENT_SCRIPT_VAR_DATE_ADVANCE, 365);
}

/* CallFunction 3: raise the Brave of every blue-team unit by 10, capped at UNIT_BRAVE_FAITH_MAX. */
void unit_increase_blue_team_brave_by_10(void) {
    unit_stats_t* unit;
    s32 unit_id;

    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        unit = unit_stats_get(unit_id);
        if ((unit->team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= UNIT_BRAVE_FAITH_MAX) {
                unit->brave = UNIT_BRAVE_FAITH_MAX;
            }
        }
    }
}

/* CallFunction 0x0f and 0x10: place the runtime unit of battle id 3 on the tile and
 * facing of battle id 1, or that of battle id 4 on battle id 2, and cancel any
 * rotation in progress. Nothing happens when the warped battle id has no runtime
 * unit.
 *
 * The parameter arrives as the CallFunction selector and is reused for the runtime
 * id. */
void script_warp_unit_display_to_paired_unit(s32 runtime_id) {
    unit_stats_t* unit;
    unit_animation_state_t* state;
    s32 facing;
    s32 battle_id;

    if (runtime_id == EVENT_CALL_FUNCTION_WARP_UNIT_4_DISPLAY_TO_UNIT_2_POSITION_AND_FACING) {
        unit = unit_stats_get(1);
        battle_id = 3;
    } else {
        unit = unit_stats_get(2);
        battle_id = 4;
    }
    runtime_id = unit_battle_runtime_id_get(battle_id);
    if (runtime_id != -1) {
        s_script_warp_coords.x = unit->x;
        s_script_warp_coords.layer = unit->position.higher_elevation;
        s_script_warp_coords.y = unit->position.y;
        facing = unit->position.facing << 2;
        state = &g_unit_animation_states[runtime_id];
        state->delay = 0;
        state->rotating = 0;
        state->target_facing = facing;
        unit_place_in_new_location(runtime_id, &s_script_warp_coords, unit->position.facing);
        unit_event_facing_set(runtime_id, unit->position.facing << 2);
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

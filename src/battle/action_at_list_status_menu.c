/* Status-menu AT list browsing with the team tints it uses. */

#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Ends the repeating tint of every unit CLUT that has an animation running and
 * fades it back to its last applied colors. */
void battle_gfx_update_unit_palettes(void) {
    s32 palette_index;

    for (palette_index = 0; palette_index < 0x10; palette_index++) {
        if (g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].animations[palette_index].active != 0) {
            map_modify_palette(MAP_COLOR_MODE_STOP_REPEAT, 4, MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
            map_modify_palette(MAP_COLOR_MODE_RESTORE, 4, MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
        }
    }
}

/* Tint a unit's palette by its team: blue, red, green or light blue.
 *
 * `amount` passes through to map_modify_palette. */
void battle_gfx_modify_palette_based_on_team(battle_unit_t* unit, s32 amount) {
    if (unit == 0) {
        return;
    }
    if (unit->stats == 0) {
        return;
    }
    switch ((unit->stats->team_flags & BATTLE_TEAM_MASK) >> BATTLE_TEAM_SHIFT) {
    case BATTLE_TEAM_BLUE >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0, 0x18);
        break;
    case BATTLE_TEAM_RED >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0x10, 0, 0);
        break;
    case BATTLE_TEAM_GREEN >> BATTLE_TEAM_SHIFT:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0);
        break;
    default:
        map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, amount, MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0x10);
        break;
    }
}

void battle_gfx_tint_all_units_by_team(void) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        battle_gfx_modify_palette_based_on_team(battle_unit_get_by_runtime_id(i), 4);
    }
}

/* Advance the AT list cursor, wrapping after the list's 40 entries. */
void action_increment_at_list_id(void) {
    g_action_at_list_id++;
    if (g_action_at_list_id >= BATTLE_AT_LIST_CAPACITY) {
        g_action_at_list_id = 0;
    }
}

/* Show the next AT list entry and advance the AT list cursor past it.
 *
 * Empty entries are skipped. A charged action moves the cursor to its target and
 * marks the tiles it will hit; a unit's turn moves the cursor to the unit and
 * removes the tile marks. The free cursor's Start button and the status-menu
 * request call it. The loop has no exit when every entry is empty. */
void action_show_next_at_list_entry(void) {
    s32 id;
    battle_unit_t* unit;

    for (;;) {
        id = action_calculate_at_list_and_get_specific_unit_id(g_action_at_list_id);
        if (id >= 0) {
            unit = battle_unit_get_by_battle_id(id & 0xFF);
            if ((id & 0x100) != 0) {
                target_move_cursor_to_action_target(unit);
                target_calculate_for_menu_types(&unit->stats->action_command);
                target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
            } else {
                target_move_cursor_to_unit(unit);
                target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
            }
            break;
        }
        action_increment_at_list_id();
    }
    action_increment_at_list_id();
}

/* Open the status menu once: mark it open and restart the AT list cursor at entry
 * 0. */
void action_open_status_menu_at_list_start(void) {
    if (g_status_menu_open == 0) {
        g_status_menu_open = 1;
        g_action_at_list_id = 0;
    }
}

void action_clear_at_list_id(void) {
    g_action_at_list_id = 0;
}

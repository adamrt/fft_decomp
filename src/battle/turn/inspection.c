/* Map cursor visibility and placement, team highlights and AT List inspection. */

#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/sprite.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

static s32 s_at_list_cursor;

/* Show the map cursor and its tile glow again; the counterpart of
 * cursor_hide. */
void cursor_show(void) {
    g_cursor_visible = 1;
}

/* Hide the map cursor and its tile glow: the flag is read only by
 * cursor_update. */
void cursor_hide(void) {
    g_cursor_visible = 0;
}

/* Move the map cursor to a unit's tile and elevation; a null unit leaves it where it
 * is. */
void cursor_move_to_unit(const unit_runtime_t* unit) {
    if (unit != 0) {
        g_cursor_x = unit->map_x;
        g_cursor_y = unit->map_y;
        g_cursor_layer = unit->map_layer;
    }
}

/* Move the cursor to the target of the unit's stored action: the target tile, or
 * the tile of the target unit. */
void cursor_move_to_action_target(const unit_runtime_t* unit) {
    unit_stats_t* stats = unit->stats;

    if (stats->action_command.targeting_type == TARGET_TYPE_TILE) {
        /* The retail code reloads unit->stats for the elevation and y reads. */
        g_cursor_x = stats->action_command.target_position.vx;
        g_cursor_layer = unit->stats->action_command.target_position.vy;
        g_cursor_y = unit->stats->action_command.target_position.vz;
        return;
    }
    cursor_move_to_unit(unit_battle_get(stats->action_command.target_id));
}

/* Stop every running color animation on the unit CLUT row and restore those palettes
 * to their original colors. */
void sprite_palette_restore_all(void) {
    s32 palette_index;

    for (palette_index = 0; palette_index < BATTLE_MAP_CLUTS_PER_ROW; palette_index++) {
        if (g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_UNITS].animations[palette_index].active != 0) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_STOP_REPEAT, 4, BATTLE_MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_RESTORE, 4, BATTLE_MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
        }
    }
}

/* Tint a unit's palette by its team: blue, red, green or light blue (green plus
 * blue). The tint is a repeating base-color pulse on the unit's own CLUT;
 * `amount` passes through to battle_map_palette_modify. A null unit or one
 * without battle data is left alone. */
void sprite_palette_tint_by_team(unit_runtime_t* unit, s32 amount) {
    if (unit == 0) {
        return;
    }
    if (unit->stats == 0) {
        return;
    }
    switch ((unit->stats->team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT) {
    case UNIT_TEAM_BLUE >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0, 0x18);
        break;
    case UNIT_TEAM_RED >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0x10, 0, 0);
        break;
    case UNIT_TEAM_GREEN >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0);
        break;
    default:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0x10);
        break;
    }
}

/* Pulse every unit's palette in its team color (sprite_palette_tint_by_team): the free
 * cursor's Square highlight and the target-flashing option of the ability preview. */
void sprite_palette_tint_all_by_team(void) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        sprite_palette_tint_by_team(unit_runtime_get(i), 4);
    }
}

/* Advance the AT list cursor, wrapping after the list's 40 entries. */
static void at_list_cursor_advance(void) {
    s_at_list_cursor++;
    if (s_at_list_cursor >= AT_LIST_CAPACITY) {
        s_at_list_cursor = 0;
    }
}

/* Show the next AT list entry and advance the AT list cursor past it.
 *
 * Empty entries are skipped. A charged action moves the cursor to its target and
 * marks the tiles it will hit; a unit's turn moves the cursor to the unit and
 * removes the tile marks. The free cursor's Start button and the status-menu
 * request call it. The loop has no exit when every entry is empty. */
void at_list_show_next_entry(void) {
    s32 id;
    unit_runtime_t* unit;

    for (;;) {
        id = at_list_get_entry_unit_id(s_at_list_cursor);
        if (id >= 0) {
            unit = unit_battle_get(id & AT_LIST_ENTRY_BATTLE_ID_MASK);
            if ((id & AT_LIST_ENTRY_CHARGED_ACTION) != 0) {
                cursor_move_to_action_target(unit);
                target_area_set_for_action(&unit->stats->action_command);
                target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
            } else {
                cursor_move_to_unit(unit);
                target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 0);
            }
            break;
        }
        at_list_cursor_advance();
    }
    at_list_cursor_advance();
}

/* Open the status menu once: mark it open and restart the AT list cursor at entry
 * 0. */
void menu_status_open(void) {
    if (g_menu_status_open == 0) {
        g_menu_status_open = 1;
        s_at_list_cursor = 0;
    }
}

/* Restart the AT list cursor at entry 0, so the free cursor's next Start press shows the first
 * entry. */
void at_list_reset_cursor(void) {
    s_at_list_cursor = 0;
}

/* Drop a pending status-menu request (Triangle during a unit's turn the player does not control)
 * and mark the status menu closed. */
void menu_status_clear(void) {
    g_menu_status_requested = 0;
    g_menu_status_open = 0;
}

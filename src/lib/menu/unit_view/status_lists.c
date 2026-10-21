/* Current-status lists and the panel record transfers used when a menu overlay publishes
 * a selected unit back to the resident event display. */

#include "fft/ability_list.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

/* current-status list */
extern u16 g_status_screen_status_list_row_actions[8];

/* Text ids of the unit's current statuses in the status panel's two columns, filled by
 * status_screen_build_status_list. */
static s16 g_unit_panel_status_left_text_ids[20];
static s16 g_unit_panel_status_right_text_ids[20];

/* Return 1 when the unit has any current status (innate ones included), else 0; one of
 * the conditions under which Circle opens the status list. */
s32 unit_has_any_current_status(s32 battle_id) {
    unit_stats_t* unit = unit_stats_get(battle_id);
    s32 index;

    for (index = 0; index < STATUS_BYTE_COUNT; index++) {
        if (unit->status_sets.current[index] != 0) {
            return 1;
        }
    }
    return 0;
}

/* Builds the two-column status list for the stored unit: every set bit of
 * the current status bytes becomes text id 0x8800 + byte * 8 + bit, then
 * the visible/overflow row counts are derived from the entry count.
 *
 * The flag reads go through local pointers so they are reloaded each
 * iteration as the target does; the columns are s16 so `index + 0x8800`
 * stays a 32-bit addition with the constant hoisted into a register. */
void status_screen_build_status_list(void) {
    unit_stats_t* unit;
    u16 i;
    u16 bit;
    u16 count;
    u16 half;
    u16 odd;
    u32 mask;
    u16 index;
    s16* flag_a;
    s16* flag_b;

    /* A generic monster's or team kind 1 unit's rows get the inert MENU_ROW_ACTION_NONE (Circle
     * only records the row); other units' rows open JOBSTTS silently. */
    for (i = 0; i < 8; i++) {
        flag_a = &g_unit_panel_selected_unit_fields.state.generic_monster;
        flag_b = &g_unit_panel_selected_unit_status.team_kind;
        if (*flag_a != 0 || *flag_b == 1) {
            g_status_screen_status_list_row_actions[i] = MENU_ROW_ACTION_NONE;
        } else {
            g_status_screen_status_list_row_actions[i] = MENU_ROW_ACTION_SILENT | MENU_COMMAND_ID_JOBSTTS;
        }
    }
    unit = unit_stats_get(g_unit_panel_selected_unit.battle_id);
    for (i = 0; i < 20; i++) {
        g_unit_panel_status_left_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_unit_panel_status_right_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_ability_list_ability_display_flags[i] = 0;
    }
    count = 0;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        mask = 0x80;
        for (bit = 0; bit < 8; bit++) {
            if (mask & unit->status_sets.current[i]) {
                index = i * 8 + bit;
                if (count & 1) {
                    g_unit_panel_status_right_text_ids[count / 2] = index + 0x8800;
                } else {
                    g_unit_panel_status_left_text_ids[count / 2] = index + 0x8800;
                }
                count++;
            }
            mask >>= 1;
        }
    }
    sound_confirm_queue();
    if (count == 0) {
        status_screen_jobs_thread();
    }
    odd = count & 1;
    half = count / 2;
    count = half + odd;
    if (count >= 8) {
        g_status_screen_status_list_layout.columns.row_count = 7;
        g_status_screen_status_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_status_screen_status_list_layout.columns.row_count = count;
        g_status_screen_status_list_layout.columns.hidden_rows = 0;
    }
    ability_list_scrolling_thread();
}

/* Copy the selected panel and tile records into four caller-owned buffers. */
void unit_panel_copy_billboard_out(unit_panel_summary_t* banner, unit_panel_unit_t* unit,
    unit_panel_record_t* billboard, battle_map_selected_tile_t* cursor_tile) {
    mem_copy_bytes_nonempty(banner, &g_unit_panel_selected_unit_status, sizeof(g_unit_panel_selected_unit_status));
    mem_copy_bytes_nonempty(unit, &g_unit_panel_selected_unit, sizeof(g_unit_panel_selected_unit));
    mem_copy_bytes_nonempty(billboard, &g_unit_panel_selected_unit_fields, sizeof(g_unit_panel_selected_unit_fields));
    mem_copy_bytes_nonempty(cursor_tile, &g_battle_map_selected_tile_data, sizeof(g_battle_map_selected_tile_data));
}

/* Load the active-turn banner, the active-turn unit record and the billboard data
 * from the caller's copies: the inverse of unit_panel_copy_billboard_out,
 * without the cursor tile record. REQUIRE.OUT publishes its own records this way. */
void unit_panel_copy_billboard_in(
    const unit_panel_summary_t* banner, const unit_panel_unit_t* unit, const unit_panel_record_t* billboard) {
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit_status, banner, sizeof(g_unit_panel_selected_unit_status));
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit, unit, sizeof(g_unit_panel_selected_unit));
    mem_copy_bytes_nonempty(&g_unit_panel_selected_unit_fields, billboard, sizeof(g_unit_panel_selected_unit_fields));
}

#include "fft/unit.h"
#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/sound.h"

s32 unit_has_any_current_status(s32 battle_id) {
    unit_stats_t* unit;
    s32 index;

    unit = battle_unit_get_stats_from_battle_id(battle_id);
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
 * stays a 32-bit addition with the constant hoisted into a register.
 *
 * BATTLE twin of menu_build_unit_status_list. */
void menu_build_unit_status_list(void) {
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

    for (i = 0; i < 8; i++) {
        flag_a = &g_menu_selected_unit_fields.state.generic_monster;
        flag_b = &g_menu_selected_unit_status.team_kind;
        if (*flag_a != 0 || *flag_b == 1) {
            g_menu_status_list_row_actions[i] = 0x7D0;
        } else {
            g_menu_status_list_row_actions[i] = MENU_ROW_ACTION_SILENT | BATTLE_MENU_ID_JOBSTTS;
        }
    }
    unit = battle_unit_get_stats_from_battle_id(g_menu_selected_unit.battle_id);
    for (i = 0; i < 20; i++) {
        g_menu_status_left_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_menu_status_right_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_menu_ability_display_flags[i] = 0;
    }
    count = 0;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        mask = 0x80;
        for (bit = 0; bit < 8; bit++) {
            if (mask & unit->status_sets.current[i]) {
                index = i * 8 + bit;
                if (count & 1) {
                    g_menu_status_right_text_ids[count / 2] = index + 0x8800;
                } else {
                    g_menu_status_left_text_ids[count / 2] = index + 0x8800;
                }
                count++;
            }
            mask >>= 1;
        }
    }
    sound_queue_sfx_confirm();
    if (count == 0) {
        menu_run_jobstts_thread();
    }
    odd = count & 1;
    half = count / 2;
    count = half + odd;
    if (count >= 8) {
        g_menu_status_list_layout.columns.row_count = 7;
        g_menu_status_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_menu_status_list_layout.columns.row_count = count;
        g_menu_status_list_layout.columns.hidden_rows = 0;
    }
    menu_run_scrolling_ability_list_thread_variant();
}

/* Copy the selected panel and tile records into four caller-owned buffers. */
void menu_copy_billboard_data_out(menu_unit_status_record_t* banner, menu_active_unit_t* unit,
    menu_status_panel_slot_storage_t* billboard, map_selected_tile_t* cursor_tile) {
    mem_copy_bytes_nonempty(banner, &g_menu_selected_unit_status, sizeof(g_menu_selected_unit_status));
    mem_copy_bytes_nonempty(unit, &g_menu_selected_unit, sizeof(g_menu_selected_unit));
    mem_copy_bytes_nonempty(billboard, &g_menu_selected_unit_fields, sizeof(g_menu_selected_unit_fields));
    mem_copy_bytes_nonempty(cursor_tile, &g_map_selected_tile_data, sizeof(g_map_selected_tile_data));
}

/* Load the active-turn banner, the active-turn unit record and the billboard data
 * from the caller's copies: the inverse of menu_copy_billboard_data_out,
 * without the cursor tile record. REQUIRE.OUT publishes its own records this way. */
void menu_copy_billboard_data_in(const menu_unit_status_record_t* banner, const menu_active_unit_t* unit,
    const menu_status_panel_slot_storage_t* billboard) {
    mem_copy_bytes_nonempty(&g_menu_selected_unit_status, banner, sizeof(g_menu_selected_unit_status));
    mem_copy_bytes_nonempty(&g_menu_selected_unit, unit, sizeof(g_menu_selected_unit));
    mem_copy_bytes_nonempty(&g_menu_selected_unit_fields, billboard, sizeof(g_menu_selected_unit_fields));
}

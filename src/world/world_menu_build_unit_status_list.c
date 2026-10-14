#include "fft/sound.h"
#include "fft/unit.h"
#include "fft/world.h"

/* Builds the two-column status list for the stored unit: every set bit of
 * the current status bytes becomes text id 0x8800 + byte * 8 + bit, then
 * the visible/overflow row counts are derived from the entry count.
 *
 * The flag reads go through local pointers so they are reloaded each
 * iteration as the target does; the columns are s16 so `index + 0x8800`
 * stays a 32-bit addition with the constant hoisted into a register. */
void world_menu_build_unit_status_list(void) {
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
            g_world_menu_status_list_row_actions[i] = 0x7D0;
        } else {
            g_world_menu_status_list_row_actions[i] = MENU_ROW_ACTION_SILENT | BATTLE_MENU_ID_JOBSTTS;
        }
    }
    unit = battle_unit_get_stats_from_battle_id(g_menu_selected_unit.battle_id);
    for (i = 0; i < 20; i++) {
        g_world_menu_status_left_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_world_menu_status_right_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_world_menu_ability_display_flags[i] = 0;
    }
    count = 0;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        mask = 0x80;
        for (bit = 0; bit < 8; bit++) {
            if (mask & unit->status_sets.current[i]) {
                index = i * 8 + bit;
                if (count & 1) {
                    g_world_menu_status_right_text_ids[count / 2] = index + 0x8800;
                } else {
                    g_world_menu_status_left_text_ids[count / 2] = index + 0x8800;
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
        g_world_menu_status_list_layout.columns.row_count = 7;
        g_world_menu_status_list_layout.columns.hidden_rows = count - 7;
    } else {
        g_world_menu_status_list_layout.columns.row_count = count;
        g_world_menu_status_list_layout.columns.hidden_rows = 0;
    }
    menu_run_scrolling_ability_list_thread();
}

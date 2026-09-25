#include "fft/battle.h"

/* Builds the two-column status list for the stored unit: every set bit of
 * the current status bytes becomes text id 0x8800 + byte * 8 + bit, then
 * the visible/overflow row counts are derived from the entry count.
 *
 * The flag reads go through local pointers so they are reloaded each
 * iteration as the target does; the columns are s16 so `index + 0x8800`
 * stays a word addition with the constant hoisted into a register.
 *
 * BATTLE twin of world_menu_build_unit_status_list. */
void battle_menu_build_unit_status_list(void) {
    battle_stats_t* unit;
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
        flag_a = &g_battle_menu_billboard_data[7];
        flag_b = &g_battle_menu_active_turn_banner.team_kind;
        if (*flag_a != 0 || *flag_b == 1) {
            g_battle_menu_status_list_row_actions[i] = 0x7D0;
        } else {
            g_battle_menu_status_list_row_actions[i] = 0x1015;
        }
    }
    unit = battle_unit_get_stats_from_battle_id(g_battle_active_turn_unit.battle_id);
    for (i = 0; i < 20; i++) {
        g_battle_menu_status_left_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_battle_menu_status_right_text_ids[i] = TEXT_ID_ABILITY_NAME_BASE;
        g_battle_menu_ability_display_flags[i] = 0;
    }
    count = 0;
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        mask = 0x80;
        for (bit = 0; bit < 8; bit++) {
            if (mask & unit->status_sets.current[i]) {
                index = i * 8 + bit;
                if (count & 1) {
                    g_battle_menu_status_right_text_ids[count / 2] = index + 0x8800;
                } else {
                    g_battle_menu_status_left_text_ids[count / 2] = index + 0x8800;
                }
                count++;
            }
            mask >>= 1;
        }
    }
    battle_sound_set_effect_to_confirm();
    if (count == 0) {
        battle_menu_option_menu_thread();
    }
    odd = count & 1;
    half = count / 2;
    count = half + odd;
    if (count >= 8) {
        g_battle_menu_status_visible_rows = 7;
        g_battle_menu_status_scroll_rows = count - 7;
    } else {
        g_battle_menu_status_visible_rows = count;
        g_battle_menu_status_scroll_rows = 0;
    }
    battle_menu_run_scrolling_ability_list_thread();
}

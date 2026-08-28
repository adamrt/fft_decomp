#include "fft/battle.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

/* Builds the skillset selection rows of text entry 0x5010 for the stored
 * unit and sizes menu entry 4 to the number of rows. */
void world_menu_build_skillset_entries(void) {
    world_menu_text_row_t* rows;
    s32 count;
    s32 i;
    s16 height;
    u8 separator;

    count = 0;
    g_world_ability_selected_skill_target = 0;
    g_world_action_menu_skillsets[0] = 0;
    g_world_menu_monster_skillset_flag = battle_menu_build_unit_action_menus(g_world_unit_view_battle_id,
        g_world_action_menu_skillsets, g_battle_menu_skillset_disabled_flags, g_battle_action_menu_row_types);
    rows = (world_menu_text_row_t*)world_text_find_entry(0x5010);
    if (battle_formula_can_unit_evade(world_unit_get_battle_stats_for_stored()) == 2) {
        for (i = 0; i < 6; i++) {
            g_battle_menu_skillset_disabled_flags[i] = 1;
            if (g_battle_action_menu_row_types[i] == ACTION_MENU_TYPE_UNKNOWN_0D) {
                g_battle_menu_skillset_disabled_flags[i] = 0;
            }
        }
    }
    /* The separator byte is materialised before the row counter in the
     * target; a hoisted constant lands after the counter's initialiser. */
    separator = 0xfa;
    i = 5;
    do {
        rows[i].terminator = separator;
        i--;
    } while (i >= 0);
    for (i = 0; i < 6; i++) {
        rows[i].terminator = 0xfa;
        if (g_world_action_menu_skillsets[i] == 0xff) {
            break;
        }
        if (g_battle_menu_skillset_disabled_flags[i] != 0) {
            rows[i].style = 4;
            g_world_menu_skillset_row_actions[i] = 0x1003;
        } else {
            rows[i].style = 0;
            g_world_menu_skillset_row_actions[i] = 3;
        }
        if (g_world_menu_monster_skillset_flag != 0) {
            g_world_text_substitution_values[count] = 0xb000;
        } else {
            g_world_text_substitution_values[count] = g_world_action_menu_skillsets[i] + 0xb000;
        }
        count++;
    }
    if (world_script_get_variable(EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED) != 0 && i < 6) {
        rows[i].style = 0;
        g_world_menu_skillset_row_actions[i] = 0x16;
        g_world_action_menu_skillsets[i] = ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
        g_world_action_menu_skillsets[i + 1] = 0xff;
        g_world_text_substitution_values[count++] = 0xb000 + ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
    }
    height = count * 16 + 16;
    rows[count - 1].terminator = 0xff;
    g_world_menu_thread_menu_data[4].max_row_index = count - 1;
    g_world_menu_thread_menu_data[4].overall_height = height;
    g_world_menu_thread_menu_data[4].window_height = height;
    g_world_menu_thread_menu_data[4].inner_height = height;
}

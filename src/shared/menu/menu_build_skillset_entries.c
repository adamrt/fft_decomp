#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/menu.h"
#include "fft/script.h"

/* Build skillset selection rows and size the selected unit's menu. */
void menu_build_skillset_entries(void) {
    menu_text_row_t* rows;
    s32 count;
    s32 i;
    s16 height;
    u8 separator;

    count = 0;
    g_menu_selected_skill_ct = 0;
    g_menu_skillsets[0] = 0;
    g_menu_skillset_build_result = battle_menu_build_unit_action_menus(g_menu_selected_unit.battle_id, g_menu_skillsets,
        g_battle_menu_skillset_disabled_flags, g_action_menu_row_types);
    rows = (menu_text_row_t*)text_find_entry(0x5010);
    if (battle_unit_get_action_restriction(battle_unit_get_attacker_data_pointer())
        == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
        for (i = 0; i < 6; i++) {
            g_battle_menu_skillset_disabled_flags[i] = 1;
            if (g_action_menu_row_types[i] == ACTION_MENU_TYPE_UNKNOWN_0D) {
                g_battle_menu_skillset_disabled_flags[i] = 0;
            }
        }
    }
    /* The separator byte is materialised before the row counter in the
     * target; a hoisted constant lands after the counter's initializer. */
    separator = 0xfa;
    for (i = 5; i >= 0; i--) {
        rows[i].terminator = separator;
    }
    for (i = 0; i < 6; i++) {
        rows[i].terminator = 0xfa;
        if (g_menu_skillsets[i] == SKILLSET_ID_NONE) {
            break;
        }
        if (g_battle_menu_skillset_disabled_flags[i] != 0) {
            rows[i].style = 4;
            g_menu_skillset_row_actions[i] = 0x1003;
        } else {
            rows[i].style = 0;
            g_menu_skillset_row_actions[i] = 3;
        }
        if (g_menu_skillset_build_result != 0) {
            g_text_substitution_values[count] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        } else {
            g_text_substitution_values[count] = g_menu_skillsets[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        }
        count++;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED) != 0 && i < 6) {
        rows[i].style = 0;
        g_menu_skillset_row_actions[i] = 0x16;
        g_menu_skillsets[i] = ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
        g_menu_skillsets[i + 1] = SKILLSET_ID_NONE;
        g_text_substitution_values[count++] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE + ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
    }
    height = count * 16 + 16;
    rows[count - 1].terminator = 0xff;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].max_row_index = count - 1;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].text_image.height = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].window.h = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].inner_height = height;
}

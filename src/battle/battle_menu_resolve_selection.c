#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "psx/types.h"

/**
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values, publishes the resolved selection, and returns
 * the command, option, and item packed into a single value.
 */
s32 battle_menu_resolve_selection(void) {
    s32 command;
    s32 option;
    u32 item;
    s32 index;
    s32 i;
    s32 menu;

    command = (u8)g_menu_pending_selection[0];
    option = (u8)g_menu_pending_selection[1];
    item = (u16)g_menu_pending_selection[2];
    g_battle_menu_selected_command.kind = command;
    index = option;
    if (command != 0x12 && (command & 0xFE) != 0xFE) {
        menu = g_menu_current_id;
        for (i = 0; i < 31; i++) {
            if (menu == g_menu_command_maps[i].menu_id) {
                if (command != 0xFF) {
                    if (menu == BATTLE_MENU_ID_DEAD_UNIT) {
                        command = g_battle_dead_unit_action;
                        /* Item selection updates only the low byte of the 16-bit command field. */
                        *(u8*)&g_battle_menu_selected_action.item_or_equipment = g_battle_dead_unit_result;
                        option = 0xFFFE;
                    } else if (menu == 0x19 || menu >= 0x64) {
                        command = *(g_menu_command_maps[i].map + option + 1);
                        option = 0xFFFE;
                    } else {
                        command = *(g_menu_command_maps[i].map + command + 1);
                    }
                } else {
                    command = g_menu_command_maps[i].map[0];
                }
                break;
            }
        }
        if (i == 31) {
            command = 7;
        }
        if (i < 16) {
            g_menu_resolved_command = command;
        } else if (command == 5 || command == 6) {
            g_battle_menu_selected_action.targeting_type = command;
        }
        if (command == 1) {
            g_menu_restore_pending = 1;
        }
        if (command == 12) {
            if (option < 3) {
                s32 option_offset = (option - 1) * 2;
                s32 adjusted_item = item + 12;

                command = option_offset + adjusted_item;
            } else {
                command = option + 13;
            }
            option = 0xFFFE;
            item = 0xFFFE;
            g_menu_resolved_command = command;
        }
        g_battle_menu_selected_command.kind = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_menu_skillsets[index];
            if (option == SKILLSET_ID_ATTACK || option == SKILLSET_ID_DEFEND || option == SKILLSET_ID_EQUIP_CHANGE
                || option == SKILLSET_ID_BLANK_04 || option == SKILLSET_ID_ELEMENTAL || option == SKILLSET_ID_JUMP) {
                g_battle_menu_selected_action.ability_id = 0;
                item = 0xFFFE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_menu_skillset_build_result == 0) {
                item = g_battle_menu_anything_ability_id;
            } else {
                item = g_ai_workspace_ptr->ability_list.ids[item] & 0x1FF;
            }
            g_battle_menu_selected_action.skillset_id = option;
            g_battle_menu_used_skillset_id = option;
            if (item != 0xFFFE) {
                if (option == SKILLSET_ID_ITEM || option == SKILLSET_ID_DRAW_OUT || option == SKILLSET_ID_THROW) {
                    *(u8*)&g_battle_menu_selected_action.item_or_equipment = item;
                } else {
                    g_battle_menu_selected_action.ability_id = item;
                }
                g_battle_menu_used_item_id = item;
            }
        }
    }
    return command | ((option & 0xFF) << 8) | (item << 16);
}

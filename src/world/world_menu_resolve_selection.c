#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* The selection slots are four halfwords. Command and option use their low
 * bytes; item uses its full halfword. */
typedef struct {
    u8 command;
    u8 _unused_01;
    u8 option;
    u8 _padding_03; /* aligns item */
    u16 item;
    u16 _unused_06;
} world_menu_selection_view_t;

/**
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values, publishes the resolved selection, and returns
 * the command, option, and item packed into a single value.
 */
s32 world_menu_resolve_selection(void) {
    world_menu_selection_view_t* selection = (world_menu_selection_view_t*)g_world_menu_pending_selection;
    s32 command;
    s32 option;
    u32 item;
    s32 index;
    s32 i;
    s32 menu;

    command = selection->command;
    option = selection->option;
    item = selection->item;
    g_world_selected_ability = command;
    index = option;
    if (command != 0x12 && (command & 0xFE) != 0xFE) {
        menu = g_world_menu_current_id;
        for (i = 0; i < 31; i++) {
            if (menu == g_world_menu_command_maps[i].menu_id) {
                if (command != 0xFF) {
                    if (menu == 0x69) {
                        command = g_world_dead_unit_action;
                        g_world_menu_preview_action.item_id = (u8)g_world_dead_unit_result;
                        option = 0xFFFE;
                    } else if (menu == 0x19 || menu >= 0x64) {
                        command = *(g_world_menu_command_maps[i].map + option + 1);
                        option = 0xFFFE;
                    } else {
                        command = *(g_world_menu_command_maps[i].map + command + 1);
                    }
                } else {
                    command = g_world_menu_command_maps[i].map[0];
                }
                break;
            }
        }
        if (i == 31) {
            command = 7;
        }
        if (i < 16) {
            g_world_menu_resolved_command = command;
        } else if ((u32)(command - 5) < 2) {
            g_world_menu_preview_action.targeting_type = command;
        }
        if (command == 1) {
            g_world_menu_restore_pending = 1;
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
            g_world_menu_resolved_command = command;
        }
        g_world_selected_ability = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_world_action_menu_skillsets[index];
            if ((u32)(option - 1) < 2 || (u32)(option - 3) < 2 || (u32)(option - 0x11) < 2) {
                g_world_menu_preview_action.ability_id = 0;
                item = 0xFFFE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_world_menu_monster_skillset_flag == 0) {
                item = g_world_menu_anything_ability_id;
            } else {
                item = ((u16*)g_battle_ai_workspace_ptr)[item] & 0x1FF;
            }
            g_world_menu_preview_action.skillset = option;
            g_world_menu_resolved_skillset = option;
            if (item != 0xFFFE) {
                if (option == 6 || (u32)(option - 0x13) < 2) {
                    g_world_menu_preview_action.item_id = item;
                } else {
                    g_world_menu_preview_action.ability_id = item;
                }
                g_world_menu_resolved_ability = item;
            }
        }
    }
    return command | ((option & 0xFF) << 8) | (item << 16);
}

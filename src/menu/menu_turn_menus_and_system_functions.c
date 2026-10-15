/* Menu frame update, unit selection records, turn/skillset/action menus, confirm threads, system function dispatch and
 * scenario conditions. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_menu_pending_selection[i] = fill;
    }
    script_run_event_frame_variant(otag, controller_input);
    menu_update_panel_fade();
    result = menu_resolve_selection();
    if ((thread_is_running_asm_variant(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm_variant(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm_variant(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_animation_speed_forced == 0) {
            g_menu_animation_speed_forced = 1;
            g_menu_saved_animation_speed = battle_state_get_animation_speed();
            script_set_event_speed_variant(2);
        }
    } else if (g_menu_animation_speed_forced == 1) {
        g_menu_animation_speed_forced = 0;
        script_set_event_speed_variant(g_menu_saved_animation_speed);
    }
    return result;
}

/* Fill g_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_main_terrain_tile_info_pairs. */
void map_store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    map_store_selected_tile_coordinates(&coords);
    surface_type = &g_map_selected_tile_data.surface_type;
    *surface_type = map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        = map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        += (map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.depth = depth;
    g_map_selected_tile_data.display_height += depth * 2;
    g_map_selected_tile_data._unknown_02 = g_main_terrain_tile_info_pairs[*surface_type][0];
    g_map_selected_tile_data._unknown_04 = g_main_terrain_tile_info_pairs[*surface_type][1];
}

void menu_clear_entry_selections(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        g_menu_thread_menu_data[g_menu_cleared_entry_indices[i]].selected_index = 0;
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_clear_entry_selections and menu_restore_selection_for_selected_unit.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_7(void) {
}

/* Restore the selected unit's remembered command row, applying menu remaps. */
void menu_restore_selection_for_selected_unit(s32 menu_id) {
    menu_unit_selection_record_t* record;
    s32 i;
    s32 row;
    s32 candidate;
    menu_selection_remap_t* remap;

    for (i = 0; i < 12; i++) {
        if (g_menu_command_maps[i].menu_id == menu_id) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    record = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    for (row = 0, remap = g_menu_selection_remaps; row < 8; remap++, row++) {
        if (menu_id == g_menu_selection_remaps[row].menu_id) {
            /* Keep the signed byte-table index used by the target. */
            for (candidate = 2; candidate < 5; candidate++) {
                if (record->command_index == remap->matches[candidate - 2]) {
                    record->command_index = g_menu_selection_remaps[row].replacement;
                    break;
                }
            }
        }
    }
    g_menu_thread_menu_data[menu_id].selected_index = record->command_index;
}

/* One row per battle unit, each 17 bytes. A typed row-array version changes the target's
 * induction variables (84 bytes rather than 76), so retain the byte view.
 * Use an integer countdown: decrementing an element pointer after row[0]
 * would form a pointer before the first row. */
enum { WORLD_MENU_UNIT_SELECTION_RECORD_BYTES = (s32)sizeof(menu_unit_selection_record_t) };

/* Reset the per-unit menu records, clearing each and marking byte 1 as
 * unset (0xFF). */
void menu_reset_unit_records(void) {
    s32 index;
    u8* row;
    s32 count;
    u8 fill;

    fill = 0xFF;
    index = 0;
    row = (u8*)g_menu_unit_selection_records;
    do {
        count = WORLD_MENU_UNIT_SELECTION_RECORD_BYTES - 1;
        do {
            row[count] = 0;
            count -= 1;
        } while (count >= 0);
        ((u8*)g_menu_unit_selection_records)[index + 1] = fill;
        index += WORLD_MENU_UNIT_SELECTION_RECORD_BYTES;
        row += WORLD_MENU_UNIT_SELECTION_RECORD_BYTES;
    } while (index < BATTLE_UNIT_SLOT_COUNT * WORLD_MENU_UNIT_SELECTION_RECORD_BYTES);
}

void menu_reset_unit_record(s32 battle_id) {
    unit_stats_t* stats;
    menu_unit_selection_record_t* records;
    s32 i;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    records = g_menu_unit_selection_records;
    for (i = 0; i < 17; i++) {
        if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
            ((u8*)&records[battle_id])[i] = 0;
            g_menu_unit_selection_records[battle_id]._unknown_01 = 0xFF;
        }
    }
}

ai_command_t* menu_get_selected_command_address(void) {
    return &g_menu_selected_command;
}

/* Post the close command after two frames under the mini-menu task id. */
void menu_mini_menu_cancel_delay_thread(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(2);
    g_menu_pending_selection[0] = 7;
    thread_exit_current();
}

/* Queue the confirm sound and hold the mini-menu task id for 16 frames. */
void menu_confirm_sound_delay_thread(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(16);
    thread_exit_current();
}

s32 menu_copy_u8_to_u16_terminated(u16* destination, const u8* source) {
    s32 count = 0;

    for (;;) {
        if (*source == 0xFF) {
            return count;
        }
        *destination = *source;
        source++;
        count++;
        destination++;
    }
}

/* Widen count bytes into 16-bit menu values; a nonpositive count writes nothing. */
void menu_copy_u8_to_u16(u16* destination, const u8* source, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        *destination = *source;
        source++;
        destination++;
    }
}

/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_ai_workspace_ptr through its own loader, then
 * the rows are sized, grayed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80166994 is set up for 6 visible rows.
 * BATTLE twin of menu_build_ability_list (0x800f32e4); the two are masked-identical.
 *
 * Matching notes: the layout block is one struct (its x/y are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. */
void menu_build_ability_list(s32 mode) {
    s32 unit_index;
    s32 window_x;
    unit_stats_t* unit;
    s32 i;
    s32 type;
    s32 skillset;
    s32 count;
    s32 found;
    s32 columns;
    s32 row_mode;
    s32 row_flags;

    g_menu_ability_layout.columns.text_ids[0] = (s16*)g_ai_workspace_ptr->ability_list.ids;
    g_menu_ability_layout.columns.text_ids[1] = (s16*)g_ai_workspace_ptr->ability_list.values;
    g_menu_ability_layout.columns.text_ids[2] = (s16*)g_ai_workspace_ptr->ability_list.extras;
    unit_index = g_menu_selected_unit.battle_id;
    unit = menu_get_selected_unit_stats();
    type = g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    skillset = g_menu_skillsets[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    g_menu_selected_action.skillset_id = skillset;
    for (i = 0; i < 0x50; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.mp_costs[i] = 0xFF;
        g_ai_workspace_ptr->ability_list.ct[i] = 1;
        g_ai_workspace_ptr->ability_list.values[i] = 0;
        g_ai_workspace_ptr->ability_list.extras[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.flags[i] = 0;
        g_ai_workspace_ptr->ability_list.turns[i] = 1;
        g_ai_workspace_ptr->ability_list.skill_ct[i] = 0xFF;
        g_menu_ability_display_flags[i] = 0;
    }
    g_ai_workspace_ptr->ability_list.mp_costs[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.ct[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.flags[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.turns[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.skill_ct[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        battle_menu_get_unit_skillset_ability_data(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.ct, 0,
            g_ai_workspace_ptr->ability_list.flags, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.ct, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        battle_menu_display_item_inventory_ability(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        battle_menu_load_throw_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == MENU_ARITHMETICKS_LIST_ATTRIBUTES) {
            battle_menu_load_math_skill_attributes(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_MULTIPLES) {
            battle_menu_load_math_skill_multiples(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_ABILITIES) {
            battle_menu_collect_calculator_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(
            unit_index, skillset, g_ai_workspace_ptr->ability_list.ids, 0, g_ai_workspace_ptr->ability_list.flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        battle_menu_load_draw_out_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        battle_menu_load_charge_skillset(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(
            g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = MENU_ARITHMETICKS_LIST_ABILITIES;
    }

    g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.x[1] = 0x6A;
    g_menu_ability_layout.columns.x[2] = 0x7E;
    g_menu_ability_layout.columns.mode[0] = MENU_TEXT_COLUMN_TEXT;
    if (g_menu_action_type_modes[type][1] == 1) { /* lists by id */
        count = menu_copy_u8_to_u16_terminated(
            g_ai_workspace_ptr->ability_list.ids, g_ai_workspace_ptr->ability_list.mp_costs);
        menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
        found = 0;
        for (i = 0; i < count; i++) {
            g_ai_workspace_ptr->ability_list.values[i]
                = g_main_item_quantities[g_ai_workspace_ptr->ability_list.ids[i]];
            if (g_ai_workspace_ptr->ability_list.values[i] == 0) {
                g_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
            window_x = 0x94;
            row_mode = 8;
        }
        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_menu_ability_layout.columns.x[1] -= 4;
        g_menu_ability_layout.columns.x[2] -= 4;
        columns = 0x11;
        if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
            columns = 0x17;
        }
    } else {
        found = 0;
        for (count = 0; count < 0x40; count++) {
            if (g_ai_workspace_ptr->ability_list.ids[count] == 0xFFFF) {
                break;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            for (i = 0; i < count; i++) {
                if (g_ai_workspace_ptr->ability_list.mp_costs[i] != 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (type == ACTION_MENU_TYPE_ARITHMETICKS && mode > 0) {
            window_x = 0x88;
            g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                menu_copy_u8_to_u16_terminated(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.mp_costs);
                window_x -= 8;
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
                row_mode = 6;
                g_menu_ability_layout.columns.x[1] -= 8;
                g_menu_ability_layout.columns.x[2] -= 8;
            } else {
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
                window_x = 0x94;
                row_mode = 5;
                g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
                g_menu_ability_layout.columns.x[1] -= 4;
                g_menu_ability_layout.columns.x[2] -= 4;
            }
        }
        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].inner_width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.w = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].text_image.width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = row_mode;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].select_text_table = columns;
    for (i = 0; i < count; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - g_ai_workspace_ptr->ability_list.mp_costs[i] < 0
                && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((g_ai_workspace_ptr->ability_list.flags[i] & 2) && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = 0;
        g_ai_workspace_ptr->ability_list.ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        g_ai_workspace_ptr->ability_list.values[0] = 0;
        g_ai_workspace_ptr->ability_list.extras[0] = 0;
        g_menu_ability_display_flags[0] = 4;
    }
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y = 0x30;
    if (count >= 7) {
        g_menu_ability_layout.columns.hidden_rows = count - 6;
        g_menu_ability_layout.columns.row_count = 6;
    } else {
        g_menu_ability_layout.columns.hidden_rows = 0;
        g_menu_ability_layout.columns.row_count = count;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y += (6 - count) * 8;
    }
    /* The menu thread's redraw, cursor and refresh parameters. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    g_thread_contexts[g_thread_current_id].function_parameter_3 = 0;
    g_thread_contexts[g_thread_current_id].function_parameter_4 = 1;
}

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * g_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits. */

/*
 * Walk the stored unit's
 * menu record slots from the top value down to zero, presenting each through
 * the inner subroutine and writing the chosen entry back into the record.
 */
void menu_select_unit_action_slots_thread(void) {
    s32 i = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    s32 menu = g_action_menu_row_types[i];
    s32 check_unit = g_battle_menu_skillset_disabled_flags[i];
    menu_unit_selection_record_t* row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    menu_list_selection_t* slot;
    u8* pair;
    s32 j;
    s16 text;

    /* Typed list indexing changes the address order; these byte views retain it. */
    for (i = (g_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
            slot = (menu_list_selection_t*)((u8*)row + (i + offsetof(menu_unit_selection_record_t, lists[4])) + i);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = slot->selected_index;
            g_menu_ability_layout.row_offset = slot->scroll_offset;
        } else {
            pair = (u8*)row
                + g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index * sizeof(menu_list_selection_t);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index
                = pair[offsetof(menu_unit_selection_record_t, lists[0].selected_index)];
            g_menu_ability_layout.row_offset = pair[offsetof(menu_unit_selection_record_t, lists[0].scroll_offset)];
            slot = (menu_list_selection_t*)(pair + offsetof(menu_unit_selection_record_t, lists));
        }
        for (j = 0; j < 6; j++) {
            if (menu != ACTION_MENU_TYPE_ARITHMETICKS || i == 0) {
                g_menu_action_slot_row_actions[j] = 0x1024;
            } else {
                g_menu_action_slot_row_actions[j] = -1;
            }
        }
        if (g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]
            == ACTION_MENU_TYPE_CHANGE_EQUIPMENT) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = 0;
            menu_run_equip_thread();
        }
        g_thread_call_target = menu_build_ability_list;
        thread_call_on_main_stack(i);
        script_pulse_tutorial_wait_value(0xFB);
        menu_run_scrolling_ability_list_thread_variant();
        slot->selected_index = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
        slot->scroll_offset = g_menu_ability_layout.row_offset;
        if (menu != ACTION_MENU_TYPE_ARITHMETICKS) {
            thread_exit_current();
        }
        if (g_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                thread_exit_current();
            }
            i += 2;
        } else {
            text = g_ai_workspace_ptr->ability_list
                       .ids[g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index]
                - 0x7000;
            if (i == 2) {
                g_menu_selected_action.calculator_type = text;
            }
            if (i == 1) {
                g_menu_selected_action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_menu_selected_action.ability_id = text;
            }
        }
    }
    thread_exit_current();
}

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
    if (battle_unit_get_action_restriction(menu_get_selected_unit_stats())
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

/* Build the skillset list, then wait for its submenu or run icon selection.
 * The completion value written to the thread's
 * second parameter is materialised before the branch, which puts `li v1,1`
 * in the beqz delay slot ahead of the 0xFC argument. */
void menu_run_skillset_thread(void) {
    s32 param_value;
    menu_unit_selection_record_t* row;
    s32* param;

    menu_check_action_restrictions();
    g_thread_call_target = menu_build_skillset_entries;
    thread_call_on_main_stack();
    param_value = 1;
    if (g_menu_skillset_build_result != 0) {
        g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = 0;
        if (g_menu_restore_pending == 0) {
            thread_start(g_thread_current_id - 1, menu_select_unit_action_slots_thread);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST], 0, 0);
        } else {
            thread_wait_frames(2);
        }
        for (;;) {
            thread_yield();
            if (thread_is_running_asm_variant(g_thread_current_id - 1) == 0) {
                thread_exit_current();
            }
        }
    }
    param = &g_thread_contexts[g_thread_current_id].function_parameter_2;
    *param = param_value;
    row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = row->skillset_index;
    g_menu_restore_depth = 1;
    script_pulse_tutorial_wait_value(0xFC);
    menu_run_icon_selection_loop();
    row->skillset_index = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    thread_exit_current();
}

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void menu_record_selection_for_selected_unit(void) {
    menu_unit_selection_record_t* records;
    s16* selected;
    s32 i;

    selected = &g_menu_selected_unit.battle_id;
    records = g_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_menu_current_id == g_menu_command_maps[i].menu_id) {
            records[*selected].command_index = g_menu_thread_menu_data[g_menu_current_id].selected_index;
        }
    }
}

/* Thread entry of the turn menus (entries 0x00, 0x0e, 0x13, 0x14, 0x20, 0x21 and
 * 0x2e-0x33 of the idle action menu table): run the icon selection loop, then
 * remember the chosen row for the active-turn unit and exit. */
void menu_build_main(void) {
    menu_entry_t* parameter;
    menu_unit_selection_record_t* record;
    s32 unit_id;

    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    unit_id = g_menu_selected_unit.battle_id;
    record = &g_menu_unit_selection_records[unit_id];
    /* A nonzero second parameter makes the icon loop return here instead of exiting. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    menu_run_icon_selection_loop();
    record->command_index = parameter->selected_index;
    thread_exit_current();
}

void sound_queue_sfx_confirm_unless_restoring(void) {
    if (g_menu_restore_pending == 0) {
        g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    }
}

/* Runs the menu's action handler on a simulated, silent Circle press with
 * option 0 selected, then clears the injected input. */
void menu_confirm_action_silently(menu_entry_t* menu) {
    g_sound_suppressed = 1;
    g_script_event_input = PSX_PAD_CIRCLE;
    menu_handle_action(menu, 0);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
}

/* Menu-opening thread 6: the Wait menu. */
void menu_wait_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)thread_get_current_parameter_1_variant();
    g_menu_wait_row_actions[0] = -2;
    g_menu_wait_row_actions[1] = -2;
    menu_confirm_action_silently(menu);
    thread_exit_current();
}

/* Thread entry of menu entry 0x22, which confirms the Move command.
 *
 * A unit with Don't Move gets the invalid sound and the linked-entry icon thread
 * (stored row action -1). In a menu whose move is already used (ids 0x0e, 0x14,
 * 0x21, 0x2f, 0x31 and 0x33) only the invalid sound plays. Otherwise the command
 * is confirmed at once, as a silent Circle press. */
void menu_confirm_move_command_thread(void) {
    s16* stored_actions;
    menu_entry_t* entry;
    s32 menu;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
    stored_actions = &g_menu_confirm_stored_unit_row_actions;
    *stored_actions = -2;
    if (menu_get_selected_unit_stats()->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
        & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)) {
        *stored_actions = -1;
        sound_queue_sfx_invalid();
        menu_icon_linked_entry_thread();
        thread_exit_current();
    }
    menu = g_menu_current_id;
    if (menu == BATTLE_MENU_ID_TURN_MOVED || menu == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu == BATTLE_MENU_ID_TURN_NO_ACT_MOVED || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED
        || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED || menu == BATTLE_MENU_ID_RIDER_TURN_NO_ACT_MOVED) {
        sound_queue_sfx_invalid();
        thread_exit_current();
    }
    sound_queue_sfx_confirm_unless_restoring();
    menu_confirm_action_silently(entry);
    thread_exit_current();
}

/*
 * Check action-menu restrictions and show the applicable warning.
 *
 * Menu groups 0x13/0x14 and 0x30/0x31 use the battle eligibility check.
 * Other menus inspect the unit's current status and last action. The packed
 * options can suppress status warnings, but messages 0x1002/0x1003 always
 * stop the current thread after any warning thread finishes.
 */
void menu_check_action_restrictions(void) {
    s32 message = 0;
    unit_stats_t* unit;
    s32 menu;
    s32 result;
    s32 mode;

    unit = menu_get_selected_unit_stats();
    menu = g_menu_current_id;
    if (menu == BATTLE_MENU_ID_TURN_ACTED || menu == BATTLE_MENU_ID_TURN_MOVED_ACTED
        || menu == BATTLE_MENU_ID_RIDER_TURN_ACTED || menu == BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED) {
        result = battle_unit_get_action_restriction(unit);
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_NONE) {
            sound_queue_sfx_invalid();
            thread_exit_current();
        }
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT) {
            message = 0x1002;
        }
        if (result == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
        }
    } else {
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING)) {
            if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_CHARGE) {
                g_text_substitution_values[0] = unit->action_command.skillset_id | TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
            } else {
                g_text_substitution_values[0] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            }
            message = 0x8001;
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PERFORMING)]
            & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) {
            message = 0x8002;
        }
    }
    if (message != 0) {
        mode = g_main_game_options.value & GAME_OPTIONS_NAVIGATION_MESSAGES_MASK;
        if (mode == GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS && (message & 0xff00) == mode) {
            sound_queue_sfx_confirm_unless_restoring();
        } else if (g_menu_restore_pending == 0) {
            sound_queue_sfx_invalid();
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            thread_wait_until_inactive(g_thread_current_id - 1);
        }
        if (message == 0x1002 || message == 0x1003) {
            thread_exit_current();
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
}

void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit) {
    s32 mode;
    s32 message;
    menu_entry_t* parameter;
    unit_stats_t* unit;

    g_menu_active_menu = menu;
    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    mode = 0;
    message = 0;
    unit = menu_get_selected_unit_stats();
    if (check_unit != 0) {
        if (battle_unit_get_action_restriction(unit) == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
            message = 0x1003;
            mode = 2;
        } else {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
                message = 0x1007;
                mode = 2;
            } else {
                mode = 2;
                message = g_menu_restriction_message_ids[menu] + 0x1000;
            }
        }
    }
    if (message == 0 && menu == ACTION_MENU_TYPE_ARITHMETICKS) {
        message = 0x8009 - value;
        mode = 1;
    }
    if (mode != 0) {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_OFF || (message & 0xff00) != 0x8000) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
        }
        if (mode == 2) {
            sound_queue_sfx_invalid();
            if (g_main_game_options.fields.navigation_messages == GAME_OPTION_ON) {
                thread_wait_until_inactive(g_thread_current_id - 1);
            }
            thread_exit_current();
        }
    } else {
        mode = g_menu_action_type_modes
            [g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]][0];
        if (mode < 0) {
            g_menu_action_slot_row_actions[0] = mode;
            menu_confirm_action_silently(parameter);
            thread_exit_current();
        }
    }
    g_menu_restore_depth = 2;
}

/* Battle twin of menu_validate_skill_selection_thread. Thread body: validates the skill chosen in menu entry 3
 * for the current menu (MP cost, blocking status, per-menu restrictions) and, when it is not usable, shows the matching
 * message before ending the thread. */
void menu_validate_skill_selection_thread(void) {
    s32 skill;
    menu_entry_t* parameter;
    s32 ct;
    s32 entry;
    s32 mp_cost;
    s32 message;
    s32 blocked;
    unit_stats_t* unit;
    menu_ability_list_t* skill_tables;

    skill = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    skill_tables = &g_ai_workspace_ptr->ability_list;
    ct = skill_tables->skill_ct[skill];
    entry = g_menu_ability_layout.columns.text_colors[0][skill];
    mp_cost = skill_tables->mp_costs[skill];
    if (ct == 0xFF) {
        ct = 0;
    }
    message = 0;
    blocked = skill_tables->flags[skill] & 2;
    unit = menu_get_selected_unit_stats();
    if (g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT && unit->mp - mp_cost < 0) {
        message = 0x800A;
        if (ct == 0) {
            message = 0x1008;
        }
    }
    g_menu_selected_skill_ct = ct;
    if ((g_menu_active_menu == ACTION_MENU_TYPE_MONSTER || g_menu_active_menu == ACTION_MENU_TYPE_DEFAULT)
        && blocked != 0
        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
        message = 0x8013;
        if (ct == 0) {
            message = 0x1009;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        if (entry != 0) {
            message = 0x100B;
        }
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_WEAPON_INVENTORY && entry != 0) {
        message = 0x100C;
    }
    if (g_menu_active_menu == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        if (entry != 0) {
            message = 0x100D;
        }
    }
    g_menu_result = -4;
    if (message != 0) {
        if (g_main_game_options.fields.navigation_messages == GAME_OPTION_OFF) {
            sound_queue_sfx_confirm_unless_restoring();
        } else {
            g_menu_modal_open = 1;
            g_menu_selected_option = 0;
            g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE].text_id = message;
            thread_start(g_thread_current_id - 1, menu_icon_linked_entry_thread);
            thread_set_parameters(g_thread_current_id - 1,
                (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE], 0, 0);
            sound_queue_sfx_invalid();
            thread_wait_until_inactive(g_thread_current_id - 1);
            g_menu_modal_open = 0;
        }
        if ((message != 0x800A && message != 0x8013) || g_menu_selected_option == -1) {
            g_menu_result = -1;
        }
    } else {
        sound_queue_sfx_confirm_unless_restoring();
    }
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

s32 text_is_spell_quote_exception_skillset(s32 skillset_id) {
    s32 i;
    s32 found = 0;

    for (i = 0; g_text_spell_quote_exception_skillsets[i] != SKILLSET_ID_NONE; i++) {
        if (g_text_spell_quote_exception_skillsets[i] == skillset_id) {
            found = 1;
            break;
        }
    }
    return found;
}

/**
 * Resolve the pending action-menu command, option, and item selection.
 *
 * Remaps menu-specific values, publishes the resolved selection, and returns
 * the command, option, and item packed into a single value.
 */
s32 menu_resolve_selection(void) {
    s32 command;
    s32 option;
    u32 item;
    s32 index;
    s32 i;
    s32 menu;

    command = (u8)g_menu_pending_selection[0];
    option = (u8)g_menu_pending_selection[1];
    item = (u16)g_menu_pending_selection[2];
    g_menu_selected_command.kind = command;
    index = option;
    if (command != 0x12 && (command & 0xFE) != 0xFE) {
        menu = g_menu_current_id;
        for (i = 0; i < 31; i++) {
            if (menu == g_menu_command_maps[i].menu_id) {
                if (command != 0xFF) {
                    if (menu == BATTLE_MENU_ID_DEAD_UNIT) {
                        command = g_menu_dead_unit_action;
                        /* Item selection updates only the low byte of the 16-bit command field. */
                        *(u8*)&g_menu_selected_action.item_or_equipment = g_menu_dead_unit_result;
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
            g_menu_selected_action.targeting_type = command;
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
        g_menu_selected_command.kind = command;
        if ((option & 0xFE) != 0xFE) {
            option = g_menu_skillsets[index];
            if (option == SKILLSET_ID_ATTACK || option == SKILLSET_ID_DEFEND || option == SKILLSET_ID_EQUIP_CHANGE
                || option == SKILLSET_ID_BLANK_04 || option == SKILLSET_ID_ELEMENTAL || option == SKILLSET_ID_JUMP) {
                g_menu_selected_action.ability_id = 0;
                item = 0xFFFE;
            } else if (option == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING && g_menu_skillset_build_result == 0) {
                item = g_menu_anything_ability_id;
            } else {
                item = g_ai_workspace_ptr->ability_list.ids[item] & 0x1FF;
            }
            g_menu_selected_action.skillset_id = option;
            g_menu_used_skillset_id = option;
            if (item != 0xFFFE) {
                if (option == SKILLSET_ID_ITEM || option == SKILLSET_ID_DRAW_OUT || option == SKILLSET_ID_THROW) {
                    *(u8*)&g_menu_selected_action.item_or_equipment = item;
                } else {
                    g_menu_selected_action.ability_id = item;
                }
                g_menu_used_item_id = item;
            }
        }
    }
    return command | ((option & 0xFF) << 8) | (item << 16);
}

/* Wait for menu/event threads, then start the selected system-function entry. */
void menu_run_system_function_thread(void) {
    s32 id;
    s32 menu_id;
    menu_system_function_t* entry;

    g_menu_system_function_thread_busy = 1;
    text_init_section_pointers_variant();
    id = g_thread_contexts[g_thread_current_id].function_parameter_1;
    if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_menu_system_function_table[id].alternate_id;
    }
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        while (thread_is_running_asm_variant(THREAD_SLOT_EVENT) != 0) {
            if (g_thread_contexts[THREAD_SLOT_EVENT].task_id == THREAD_TASK_EXECUTE_EVENT) {
                thread_exit_current();
            }
            thread_yield();
        }
    }
    camera_wait_until_idle();
    entry = &g_menu_system_function_table[id];
    menu_id = entry->menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].text_id = entry->text_id;
    g_menu_system_function_row_actions[0] = entry->row_action;
    g_menu_current_id = menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].thread_count = entry->thread_count;
    if (id == 0x1f) {
        thread_start(THREAD_SLOT_MENU, menu_open_option_dead_unit_panel);
        thread_set_parameters(THREAD_SLOT_MENU, 0, g_menu_dead_unit_panel_parameter, 0);
        thread_exit_current();
    }
    if (id != 8) {
        script_pulse_tutorial_wait_value(id);
    }
    thread_start(THREAD_SLOT_MENU, entry->thread_entry);
    thread_set_four_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[entry->menu_entry_index], 0, 0, 0);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
}

/* Clear menu selections and start the system-function worker. */
void menu_start_system_function_thread(s32 system_function) {
    menu_clear_entry_selections();
    thread_start(THREAD_SLOT_SYSTEM_FUNCTION, menu_run_system_function_thread);
    thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, system_function, 0, 0);
}

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void menu_confirm_auto_battle_selection_thread(void) {
    unit_stats_t* attacker;
    s32 menu;
    menu_entry_t* parameter;

    parameter = (menu_entry_t*)thread_get_current_parameter_1_variant();
    attacker = menu_get_selected_unit_stats();
    menu = g_menu_thread_menu_data[BATTLE_MENU_ID_AUTO_BATTLE].selected_index;
    if (menu == 3) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_FADING_LIFE;
    } else if (menu == 4) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_RETREAT;
    }
    g_menu_set_auto_battle_row_actions = -3;
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}

/* Store a unit's auto-battle target together with the resolved menu command as
 * its auto-battle setting. */
void menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id) {
    unit_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    stats->auto_battle_target = target_id;
    stats->auto_battle_setting = g_menu_resolved_command;
}

/* Chooses whether an ability announcement shows a spell quote.
 *
 * Each scene binds its own quote history and menu dispatcher. Unknown
 * skillsets (not in the list and not the unit's flagged primary/secondary)
 * get quote command 5 unless the ability repeats the last quote
 * (g_text_spell_quote_last_ability_id), the unit is a Calculator or Mime, the ability is >= 0x170 or
 * lacks the spell-quote flag, status blocks it, options disable it, or
 * EVENT_SCRIPT_VAR_RANDOM_VALUE is 4 or more. Everything else uses command 7. */
void text_select_ability_announcement(action_command_t* action, s32 unit_id, s32 enabled) {
    ability_data_t* ability;
    main_ability_secondary_pointer_t secondary;
    unit_stats_t* unit;
    s32 known;

    main_ability_calculate_pointers_and_type(action->ability_id & ABILITY_ID_MASK, &ability, &secondary);
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (action->skillset_id != 0) {
        known = text_is_spell_quote_exception_skillset(action->skillset_id);
    } else {
        known = 1;
    }
    if (action->skillset_id == unit->primary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_PRIMARY)) {
        known = 1;
    }
    if (action->skillset_id == unit->secondary_skillset
        && (unit->position.spell_quote_skillsets & UNIT_SPELL_QUOTE_SUPPRESS_SECONDARY)) {
        known = 1;
    }
    if (known == 0 && g_text_spell_quote_last_ability_id != (action->ability_id & ABILITY_ID_MASK)
        && unit->job_id != JOB_ID_CALCULATOR && unit->job_id != JOB_ID_MIME
        && action->ability_id < ABILITY_ID_ITEM_FIRST
        && (secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_SPELL_QUOTE)
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & (STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        && g_main_game_options.fields.ability_names == GAME_OPTION_ON
        && (script_get_variable(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 0xF) < 4) {
        g_text_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE, 0, unit_id, 0, enabled, action);
        return;
    }
    if (action->skillset_id != 0) {
        known = text_is_spell_quote_exception_skillset(action->skillset_id);
    } else {
        known = 0;
    }
    if ((known == 0 && (ability->type_flags & ABILITY_TYPE_FLAG_SHOW_NAME)
            && g_main_game_options.fields.ability_names == GAME_OPTION_ON)
        || action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, enabled, action);
    } else {
        menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
    }
}

/* Run a system function (battle_menu_system_command_e) for unit_id, looking up
 * the unit's command action first except for SCENARIO_CHECK and CLOSE.
 *
 * SPELL_QUOTE goes to text_select_ability_announcement, a void routine whose
 * leftover v0 is returned. ABILITY_ANNOUNCEMENT drops option, parameter and
 * enabled. Everything else returns menu_dispatch_system_function's result;
 * for SCENARIO_CHECK and CLOSE the action pointer passed on is unset. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    action_command_t* action;

    if (type != BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK && type != BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        action = battle_unit_get_runtime_action_command_by_battle_id(unit_id);
        if (type == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            /* The target returns the void callee's leftover $v0. */
            return ((s32 (*)(action_command_t*, s32, s32))text_select_ability_announcement)(action, unit_id, enabled);
        }
        if (type == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            return menu_dispatch_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
        }
    }
    return menu_dispatch_system_function(type, option, unit_id, parameter, enabled, action);
}

/* Run battle_unit_generate_crystal_or_treasure for the unit on the main stack,
 * keep its record in g_battle_dead_unit_context and return the treasure item when
 * the result is 4, otherwise -1. */
s32 menu_get_dead_unit_selection(s32 battle_id) {
    unit_stats_t* stats;
    crystal_pickup_result_t* result;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))battle_unit_generate_crystal_or_treasure;
    result = (crystal_pickup_result_t*)thread_call_on_main_stack(stats);
    g_menu_dead_unit_context = result;
    if (result->result == -1 || result->result == 0) {
        main_system_report_stub(0x11, 7);
    }
    /* Reload the published context after the call; reusing result changes the frame and registers. */
    if (g_menu_dead_unit_context->result != 4) {
        return -1;
    }
    return g_menu_dead_unit_context->treasure_item;
}

/* Dispatch an action-menu command to its scene system function or event. */
s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, action_command_t* action) {
    unit_stats_t* unit;
    s32 value;
    s32 count;
    s32 i;
    item_data_t* item;
    menu_system_function_t* entry;
    s32 flags;
    u8 item_id;

    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_current_id = BATTLE_MENU_ID_NONE;
    g_menu_restore_pending = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (command == BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK) {
        value = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (value >= EVENT_ID_TUTORIAL_FIRST && value <= EVENT_ID_TUTORIAL_LAST) {
            if (option != 2) {
                menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            if (option != 2) {
                menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
            }
            return MENU_SYSTEM_DISPATCH_DONE;
        } else if (option != 1) {
            if (thread_is_running_asm_variant(THREAD_SLOT_EVENT) != 0) {
                main_system_report_stub(7, 3);
                if (option != 2) {
                    menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                }
                return MENU_SYSTEM_DISPATCH_DONE;
            } else {
                count = 0;
                g_script_condition_unit_battle_id = unit_id;
                for (i = 0; i < 3; i++) {
                    if (battle_unit_find_slot_by_character_identity(i + 1) == BATTLE_CHARACTER_SLOT_UNAVAILABLE) {
                        count++;
                    }
                }
                if ((action_check_battle_outcome() < 0 || count != 0)
                    && script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
                    if (option != 2) {
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_GAME_OVER);
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        g_script_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_GAME_OVER;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                }
                if (g_script_scenario_event_active != 0) {
                    if (script_run_scenario_conditions() != 0) {
                        if (option != 2) {
                            menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                            g_menu_overlay_state = 1;
                            script_start_current_event_thread();
                        } else {
                            g_script_next_event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, value);
                        }
                        return MENU_SYSTEM_DISPATCH_EVENT;
                    }
                    if (option != 2) {
                        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                } else if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                    if (option != 2) {
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_DEBUG_BATTLE_START);
                        } else {
                            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_START);
                        }
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_START;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else if (action_check_battle_outcome() == 0) {
                    if (option != 2) {
                        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
                        g_menu_overlay_state = 1;
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_BATTLE_WON);
                        g_script_scenario_event_active = 1;
                        script_start_current_event_thread();
                    } else {
                        g_script_next_event_id = EVENT_ID_BATTLE_WON;
                    }
                    return MENU_SYSTEM_DISPATCH_EVENT;
                } else {
                    if (option != 2) {
                        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
                    }
                    return MENU_SYSTEM_DISPATCH_DONE;
                }
            }
        }
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (enabled == 0) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        return MENU_SYSTEM_DISPATCH_DONE;
    }
    if (command == BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT) {
        value = 8;
        if (g_menu_resolved_command == 0 || g_menu_resolved_command == 0x19) {
            value = 7;
            if (option == 2) {
                value = 0x31;
            }
        } else if (g_menu_resolved_command == 1) {
            g_menu_restore_pending = 1;
            value = 6;
            if (option == 1) {
                value = 0x29;
            }
        } else if (g_menu_resolved_command == 0xC) {
            value = 0x14;
        } else if (g_menu_resolved_command == 0xE) {
            value = 0x15;
        } else if (g_menu_used_skillset_id == SKILLSET_ID_DEFEND) {
            value = 0x1C;
        }
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK) {
        sound_queue_sfx_confirm();
        if (option == 0) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else if (option == 1) {
            menu_start_system_function_thread(0);
        } else if (option == 4) {
            menu_start_system_function_thread(0x30);
        } else {
            sound_queue_sfx_invalid();
            menu_start_system_function_thread(option + 0xD);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU) {
        if (g_menu_system_function_command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK
            && g_menu_system_function_option == 1) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_WAIT_MENU);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM) {
        value = 0x2F;
        if (option != 5) {
            value = 0xB;
            if (option != 1) {
                if (option == 2 || option == 4) {
                    if (g_menu_used_item_id == 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2D;
                    } else {
                        value = 0xC;
                    }
                } else {
                    if (g_menu_used_item_id != 0xFFFE || g_menu_selected_skill_ct == 0) {
                        value = 0x2E;
                    } else {
                        value = 0xD;
                    }
                }
            }
        }
        g_menu_restore_pending = 1;
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
        g_text_spell_quote_last_ability_id = action->ability_id & ABILITY_ID_MASK;
        thread_start(THREAD_SLOT_SYSTEM_FUNCTION, text_run_spell_quote_thread_variant);
        thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, unit_id, g_text_spell_quote_last_ability_id, 0);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
        text_set_message_duration_frames(0x3C);
        if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_ARITHMETICKS) {
            g_text_substitution_values[1] = unit->action_command.calculator_multiplier + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[0] = unit->action_command.calculator_type + TEXT_ID_ABILITY_NAME_BASE;
            g_text_substitution_values[2] = unit->action_command.ability_id + TEXT_ID_ABILITY_NAME_BASE;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT);
        } else {
            if (action->skillset_id == SKILLSET_ID_ITEM || action->skillset_id == SKILLSET_ID_THROW) {
                item_id = action->item_or_equipment;
                g_text_substitution_values[0] = item_id + TEXT_ID_ITEM_NAME_BASE;
            } else {
                value = action->ability_id & ABILITY_ID_MASK;
                if (value >= ABILITY_ID_SUMMON_MAGIC_MOOGLE && value <= ABILITY_ID_DRAW_OUT_CHIRIJIRADEN) {
                    g_text_substitution_values[0] = value + 0xB7C4;
                } else {
                    g_text_substitution_values[0] = value + TEXT_ID_ABILITY_NAME_BASE;
                }
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM) {
        value = 8;
        if (option == 1) {
            value = 0x16;
        } else if (option == 2) {
            value = 0x17;
        } else if (option == 3) {
            value = 0x20;
        } else if (g_menu_resolved_command == 0xC) {
            value = 0x1A;
        } else if (g_menu_resolved_command == 0xE) {
            value = 0x1B;
        }
        menu_start_system_function_thread(value);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE) {
        if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        } else {
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE].text_id = option;
            g_text_substitution_values[0] = parameter & 0xFF;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED) {
        g_text_substitution_values[0] = option + TEXT_ID_ABILITY_NAME_BASE;
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_ABILITY_LEARNED);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT) {
        menu_start_system_function_thread(option + BATTLE_SYSTEM_FUNCTION_TRAP_RESULT_FIRST);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM) {
        if (main_item_get_total_equipment_quantity(option & 0xFF, 1) == 0x63) {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_text_substitution_values[1] = option;
            item = main_item_get_data_pointer(option);
            g_text_substitution_values[2] = item->price >> 2;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, (item->price >> 2) + script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS));
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD);
        } else {
            main_item_get_data_pointer(option);
            g_text_substitution_values[0] = option;
            g_main_item_quantities[option]++;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM);
        }
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT) {
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT);
    } else if (command == BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT) {
        entry = &g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT];
        flags = parameter;
        if (option >= ACTION_DISPLAY_IMAGE_CT && option <= ACTION_DISPLAY_IMAGE_LEVEL) {
            g_text_substitution_values[0] = unit_id;
            g_text_substitution_values[1] = option + 0x506B;
            if (option == ACTION_DISPLAY_IMAGE_LEVEL) {
                flags = 1;
                if (parameter & ACTION_STAT_CHANGE_INCREASE) {
                    flags = 1 | ACTION_STAT_CHANGE_INCREASE;
                }
            }
            g_text_substitution_values[2] = flags;
            if (flags & ACTION_STAT_CHANGE_INCREASE) {
                g_text_substitution_values[2] = flags & ACTION_STAT_CHANGE_VALUE_MASK;
                entry->text_id = 0x508C;
            } else {
                entry->text_id = 0x508D;
            }
            if (option == ACTION_DISPLAY_IMAGE_CT) {
                if (flags == 0xFF) {
                    entry->text_id = 0x509A;
                } else if (flags == 0x7F) {
                    entry->text_id = 0x5099;
                }
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_GIL) {
            g_text_substitution_values[0] = parameter < 0 ? -parameter : parameter;
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508F;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_EXP) {
            g_text_substitution_values[0] = parameter & 0x7F;
            g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x508E;
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else if (option == ACTION_DISPLAY_IMAGE_STOLEN || option == ACTION_DISPLAY_IMAGE_BROKEN) {
            if (parameter & 0x8000) {
                g_text_substitution_values[0] = (parameter & 0xFF) + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = 0x509B;
            } else {
                g_text_substitution_values[0] = parameter + TEXT_ID_ITEM_NAME_BASE;
                g_menu_system_function_table[BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT].text_id = option + 0x5074;
            }
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT);
        } else {
            menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        }
    } else {
        main_system_report_stub(7, 5);
        menu_start_system_function_thread(BATTLE_SYSTEM_FUNCTION_CLOSE);
        g_menu_system_function_command = command;
        g_menu_system_function_option = option;
        return MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND;
    }
    g_menu_system_function_command = command;
    g_menu_system_function_option = option;
    return MENU_SYSTEM_DISPATCH_DONE;
}

/* Resolves an event unit id to battle stats for the scenario conditions; returns
 * -1 for none.
 *
 * An id found among the ENTD unit ids gives that unit when it has a unit record.
 * Otherwise an id in the special-character identity range is looked up by
 * character identity and accepted only for a deployed party member (unit id
 * 0x78..0x7c). */
unit_stats_t* unit_get_stats_by_unit_id(s32 unit_id) {
    s32 index;
    unit_stats_t* stats;

    battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
    if (index < 0) {
        if (unit_id < CHARACTER_IDENTITY_RAMZA_CHAPTER_1 || unit_id >= CHARACTER_IDENTITY_GENERIC_FIRST) {
            return (unit_stats_t*)-1;
        }
        index = battle_unit_find_slot_by_character_identity(unit_id);
        if (index >= 0) {
            stats = battle_unit_get_stats_from_battle_id(index);
            if (stats->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && stats->unit_id < EVENT_UNIT_ID_DEPLOYED_END) {
                return stats;
            }
        }
        return (unit_stats_t*)-1;
    }
    if (battle_unit_get_runtime_id_by_battle_id(index) >= 0) {
        return battle_unit_get_stats_from_battle_id(index);
    }
    return (unit_stats_t*)-1;
}

/* Walk the scenario's condition chains and report whether one selects an event.
 *
 * g_main_scenario_condition_offsets holds up to ten byte offsets into
 * g_main_scenario_condition_data, one per chain; a zero offset after the first
 * ends the list. A chain is a run of records, each a 16-bit type followed by
 * g_script_condition_param_lengths[type] 16-bit operands. Records are
 * evaluated while they pass and the first failure drops the chain; the RUN_EVENT
 * record that closes a chain stores its event id in the current-event variable,
 * and the function returns 1. It returns 0 when no chain gets that far.
 *
 * The checker always receives the five 16-bit values at the record, whatever the
 * type's length. Called at EventEnd and by the scenario-check command of
 * the scene's menu system-function dispatcher. */
s32 script_run_scenario_conditions(void) {
    s16* condition;
    s32 i;
    s32 shifted_offset;
    s32 offset;
    s32 result;

    for (i = 0; i < 10; i++) {
        /* The entry is a 16-bit byte offset into the script area; the
         * target sign-extends it with a live `<< 16` value, taking the
         * division's sign bit from that value rather than from the
         * sign-extended one. Spelled as `offset = (s16)g_main_scenario_condition_offsets[i]` with
         * `offset / 2`, combine folds the shift pair into a single `lh` and the
         * function comes out three instructions short, so the shift pair
         * and the by-two division are spelled out here. */
        shifted_offset = g_main_scenario_condition_offsets[i] << 16;
        offset = shifted_offset >> 16;
        if (i != 0 && offset == 0) {
            return 0;
        }
        condition = &g_main_scenario_condition_data[(offset + (s32)((u32)shifted_offset >> 31)) >> 1];
        do {
            result
                = script_check_scenario_condition(condition[0], condition[1], condition[2], condition[3], condition[4]);
            condition += g_script_condition_param_lengths[condition[0]] + 1;
        } while (result == EVENT_SCENARIO_CONDITION_RESULT_PASS);
        if (result == EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT) {
            return 1;
        }
    }
    return 0;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_run_scenario_conditions and script_check_scenario_condition. Nothing
 * on the disc calls it or points at it; the name is a neutral placeholder
 * until a reference turns up. */
void script_noop_9(void) {
}

/* Evaluate one scenario condition record for
 * script_run_scenario_conditions.
 *
 * The record's 16-bit values arrive as type (event_scenario_condition_e), id and
 * value; the two tile conditions also read a y and an elevation, with value as
 * the x. Returns EVENT_SCENARIO_CONDITION_RESULT_PASS when the condition holds,
 * RESULT_RUN_EVENT for type RUN_EVENT after storing id as the current event, and
 * RESULT_FAIL otherwise, including for every type that has no test here.
 *
 * ACTING_UNIT looks at the unit the check runs for
 * (g_script_condition_unit_battle_id): it passes when that unit's unit id
 * is id, or when it is a deployed unit whose character identity is id and id is a
 * named (non-generic) identity. While the battle id is 0xff the condition fails.
 * ITEM_EQUIPPED searches the seven equipment slots of the units with unit ids 1
 * to 3. The HP percentage is hp * 100 / (max_hp + 1). The casualty conditions
 * compare with id, not value.
 *
 * Keep the two unit-id range comparisons: retail reloads the byte. */
event_scenario_condition_result_e script_check_scenario_condition(
    event_scenario_condition_e type, s32 id, s32 value, s32 tile_y, s32 elevation) {
    unit_stats_t* unit;
    s32 i;
    s32 j; /* equipment slot, then battle index: one variable (a0 in both loops) */

    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_EQUAL && script_get_variable(id) == value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_LEAST && script_get_variable(id) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_VARIABLE_AT_MOST && value >= script_get_variable(id))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type >= 4 && type <= 10) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1) {
            if (type == EVENT_SCENARIO_CONDITION_UNIT_PRESENT)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_LEAST && unit->hp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_AT_MOST && value >= unit->hp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_LEAST
                && unit->hp * 100 / (unit->max_hp + 1) >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_HP_PERCENT_AT_MOST
                && value >= unit->hp * 100 / (unit->max_hp + 1))
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_LEAST && unit->mp >= value)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
            if (type == EVENT_SCENARIO_CONDITION_UNIT_MP_AT_MOST && value >= unit->mp)
                return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_ACTING_UNIT && g_script_condition_unit_battle_id != 0xff) {
        unit = battle_unit_get_stats_from_battle_id(g_script_condition_unit_battle_id);
        if (unit->unit_id == id
            || (unit->unit_id >= EVENT_UNIT_ID_DEPLOYED_FIRST && unit->unit_id < EVENT_UNIT_ID_DEPLOYED_END
                && unit->character_identity == id && id >= 0 && id < CHARACTER_IDENTITY_GENERIC_FIRST
                && id != CHARACTER_IDENTITY_ENTD_NONE))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
    }
    if (type == EVENT_SCENARIO_CONDITION_ITEM_EQUIPPED) {
        for (i = 1; i < 4; i++) {
            unit = unit_get_stats_by_unit_id(i);
            if (unit != (unit_stats_t*)-1) {
                for (j = 0; j < 7; j++) {
                    if (unit->equipment[j] == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) >= value)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_WAR_FUNDS_AT_MOST && value >= script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_BEFORE) {
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) < id)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && value >= script_get_variable(EVENT_SCRIPT_VAR_DAY))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_DATE_ON_OR_AFTER) {
        if (id < script_get_variable(EVENT_SCRIPT_VAR_MONTH))
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
        if (script_get_variable(EVENT_SCRIPT_VAR_MONTH) == id && script_get_variable(EVENT_SCRIPT_VAR_DAY) >= value)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_LEAST && script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES) >= id)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_CASUALTIES_AT_MOST && id >= script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES))
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_ENEMIES_DEFEATED && action_check_battle_outcome() == 0)
        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    if (type == EVENT_SCENARIO_CONDITION_UNIT_AT_TILE) {
        unit = unit_get_stats_by_unit_id(id);
        if (unit != (unit_stats_t*)-1 && unit->x == value && unit->position.y == tile_y
            && unit->position.higher_elevation == elevation)
            return EVENT_SCENARIO_CONDITION_RESULT_PASS;
    }
    if (type == EVENT_SCENARIO_CONDITION_PALETTE_UNIT_AT_TILE) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            if (battle_unit_has_runtime_id(i)) {
                j = battle_unit_get_battle_id_by_runtime_id(i);
                if (j != -1) {
                    unit = battle_unit_get_stats_from_battle_id(j);
                    if (unit->x == value && unit->position.y == tile_y && unit->position.higher_elevation == elevation
                        && unit->sprite_palette == id)
                        return EVENT_SCENARIO_CONDITION_RESULT_PASS;
                }
            }
        }
    }
    if (type == EVENT_SCENARIO_CONDITION_RUN_EVENT) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id);
        return EVENT_SCENARIO_CONDITION_RESULT_RUN_EVENT;
    }
    return EVENT_SCENARIO_CONDITION_RESULT_FAIL;
}

/* Return the current event's 16-bit finish-operation value, or 0 while no scenario event
 * is active. Bits 0-7 and 10-11 hold the next scenario id and the remaining bits
 * the after-event code. */
s32 script_get_current_scenario_finish_operation(void) {
    if (g_script_scenario_event_active == 0) {
        return 0;
    }
    return g_main_scenario_event_finish_operations[script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}

/* Return the after-event code of the pending event (g_battle_next_event_id): bits
 * 8-9 and 12-15 of its 16-bit finish-operation value, shifted down by 8. Zero means the
 * event has none. */
u32 script_get_event_finish_operation(void) {
    return (g_main_scenario_event_finish_operations[g_script_next_event_id] & 0xF300) >> 8;
}

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void script_pulse_tutorial_wait_value(s32 wait_value) {
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        thread_yield();
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}

/* Apply disabled and forced input variables while retaining the high input bits. */
void script_apply_input_overrides(u32* state) {
    u32 high;

    if (*state == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}

#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

/* Reset the selected row of the 11 menu entries listed in
 * g_battle_menu_idle_action_flag_clear_ids. */
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

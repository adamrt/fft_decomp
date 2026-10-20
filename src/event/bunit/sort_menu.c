/* Map listed units into grid cells and run the unit-order menu that changes
 * their ordering while preserving grouped monster entries. */

#include "fft/menu.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "psx/types.h"

/* reorder list */
extern u8 g_unit_list_reorder_list_initialized;
extern menu_entry_t g_unit_list_reorder_list_desc;
extern s16 g_unit_list_reorder_list_entries[];
extern s16 g_unit_list_reorder_list_redraw_request;

/* Map each listed unit to a grid cell: an ordinary unit takes the next cell in list
 * order; a mount (UNIT_MOUNT_INFO_FLAG_MOUNT) gets the list index of its rider, found
 * through the partner battle id in its mount info. The rider's list index equals its
 * cell only while no mount precedes it in the list.
 */
void unit_list_build_index_map(void) {
    s32 reorderable_index = 0;
    s32 unit_index;

    for (unit_index = 0; unit_index < g_unit_list_visible_count; unit_index++) {
        unit_list_record_t* unit = g_unit_list_pointers[unit_index];

        if (!(unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            g_unit_list_index_map[unit_index] = reorderable_index;
            reorderable_index++;
        } else {
            g_unit_list_index_map[unit_index]
                = unit_list_find_by_battle_id((unit->mount_info - UNIT_MOUNT_INFO_FLAG_MOUNT));
        }
    }
}

/* List the units that get a grid cell of their own, in list order, and return their
 * count: every listed unit except a mount, which is drawn in its rider's cell.
 */
s32 unit_list_build_sortable(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < g_unit_list_visible_count; i++) {
        if ((g_unit_list_pointers[i]->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) == 0) {
            g_unit_list_reorderable_indices[count] = i;
            count++;
        }
    }
    return count;
}

/*
 * Per-frame handler of the unit list's sort-order menu.
 *
 * The menu lists the 11 sort keys in their current priority order. Choosing
 * a row moves that key to the front, shifting the rows above it down one,
 * then stores the order back and re-sorts and rebuilds the unit list. The
 * handler returns to the unit list when the menu thread ends.
 */
void unit_list_sort_menu_update(void) {
    s16 opts[16];
    s16* src;
    s16* dst;
    s32 i;
    s16 result;
    s32 sel;
    s32 keep;

    if (g_unit_list_reorder_list_initialized == 0) {
        g_unit_list_reorder_list_desc.selected_index = 0;
        unit_list_load_sort_order(1, g_unit_list_reorder_list_entries);
        /* The rows show text entry key + 1 of the menu section, in priority order. */
        i = 0;
        src = g_unit_list_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < UNIT_LIST_SORT_KEY_COUNT);
        opts[UNIT_LIST_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_unit_list_text_menu_section, g_unit_list_text_reorder_list_buffer, opts, 1);
        g_unit_list_reorder_list_initialized = 1;
    }

    if (unit_list_update_menu_thread(UNIT_LIST_THREAD_SLOT_REORDER_MENU, &g_unit_list_reorder_list_desc) == 0) {
        g_unit_list_reorder_list_initialized = 0;
        g_unit_list_current_menu = UNIT_LIST_MENU_UNIT_LIST;
    }

    /* Each key's help text, from entry 2 of HELP_TEXT_SECTION_MENU_COMMANDS (0x1000) on. */
    g_text_selection_id = g_unit_list_reorder_list_entries[g_unit_list_reorder_list_desc.selected_index] + 0x1002;
    result = g_unit_list_reorder_result;
    if (result != -1) {
        sel = result;
        keep = g_unit_list_reorder_list_entries[sel];
        for (sel--; sel >= 0; sel--) {
            g_unit_list_reorder_list_entries[sel + 1] = g_unit_list_reorder_list_entries[sel];
        }
        g_unit_list_reorder_list_entries[0] = keep;
        unit_list_reset_results();
        i = 0;
        src = g_unit_list_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < UNIT_LIST_SORT_KEY_COUNT);
        opts[UNIT_LIST_SORT_KEY_COUNT] = -1;
        text_concatenate_entries(g_unit_list_text_menu_section, g_unit_list_text_reorder_list_buffer, opts, 1);
        g_unit_list_reorder_list_redraw_request = 1;
        unit_list_store_sort_order(1, g_unit_list_reorder_list_entries);
        unit_list_sort(1);
        unit_list_build_sortable();
        unit_list_build_index_map();
    }
}

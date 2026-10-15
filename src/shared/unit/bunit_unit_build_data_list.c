#include "fft/event_bunit.h"
#include "psx/types.h"

/* Copy every battle unit that is not an egg, crystal or treasure into the next
 * status billboard record, store the shown-plus-egg unit count in each
 * record and sort the shown list; returns the number of shown units. `mode`
 * and `unit_data` are not read. */
s32 bunit_unit_build_data_list(s32 mode, s32 unit_data, s32 sort_mode) {
    unit_stats_t* unit;
    s32 i;
    s32 shown;
    s32 total;

    shown = 0;
    total = 0;
    /* Keep status-byte reads separate; GCC otherwise merges the two tests into one 16-bit load. */
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_get_stats_from_battle_id(i);
        if ((unit != 0) && (unit->entd_slot != BATTLE_ENTD_SLOT_NONE)) {
            if (unit->unit_flags & UNIT_FLAG_EGG) {
                total++;
            } else if (!(*&unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                           & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                total++;
                bunit_unit_build_record_from_stats(unit, &g_bunit_unit_records[shown], shown);
                g_bunit_shown_unit_indices[shown] = shown;
                shown++;
            }
        }
    }

    for (i = 0; i < shown; i++) {
        g_bunit_unit_records[i].unit_count = total;
    }
    g_bunit_unit_count = shown;
    bunit_unit_sort_index_list_by_order_keys(sort_mode);
    return shown;
}

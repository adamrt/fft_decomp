#include "fft/event_bunit.h"
#include "psx/types.h"

/* Copy every battle unit that is not an egg, crystal or treasure into the next
 * status billboard record, store the shown-plus-egg unit count in each
 * record and sort the shown list; returns the number of shown units. `mode`
 * and `unit_data` are not read. */
s32 bunit_panel_build_unit_billboard_list(s32 mode, s32 unit_data, s32 sort_mode) {
    battle_stats_t* unit;
    s32 i;
    s32 shown;
    s32 total;
    s32 record_offset;
    s32 count_offset;
    s32 unused[2]; /* unreferenced local retained by the original frame layout */
    s16* shown_index;

    shown = 0;
    total = 0;
    i = 0;
    shown_index = g_bunit_shown_unit_indices;
    record_offset = 0;
    /* Keep status-byte reads separate; GCC otherwise merges the two tests into one halfword load. */
    do {
        unit = battle_unit_get_stats_from_battle_id(i);
        if ((unit != 0) && (unit->entd_slot != BATTLE_ENTD_SLOT_NONE)) {
            if (unit->unit_flags & UNIT_FLAG_EGG) {
                total++;
            } else if (!(*(volatile u8*)&unit->status_sets.current[0]
                           & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL))
                && !(unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE))) {
                total++;
                bunit_panel_copy_unit_data_to_billboard(
                    unit, (bunit_unit_data_t*)(g_bunit_unit_records + record_offset), shown);
                *shown_index = shown;
                shown_index++;
                record_offset += sizeof(bunit_unit_data_t);
                shown++;
            }
        }
        i++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);

    i = 0;
    if (shown > 0) {
        count_offset = 0;
        do {
            ((bunit_unit_data_t*)(g_bunit_unit_records + count_offset))->unit_count = total;
            i++;
            count_offset += sizeof(bunit_unit_data_t);
        } while (i < shown);
    }
    g_bunit_unit_count = shown;
    bunit_unit_sort_index_list_by_order_keys(sort_mode);
    return shown;
}

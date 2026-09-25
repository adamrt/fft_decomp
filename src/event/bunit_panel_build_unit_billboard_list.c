#include "fft/battle.h"
#include "fft/bunit.h"
#include "fft/status.h"
#include "fft/unit_flags.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

typedef struct {
    u8 character_identity; /* 0x00 */
    u8 entd_slot;          /* 0x01 */
    u8 _pad02[4];          /* 0x02 */
    u8 unit_flags;         /* 0x06 */
    u8 _pad07[0x51];       /* 0x07 */
    u8 current_status_1;   /* 0x58 */
    u8 current_status_2;   /* 0x59 */
} bunit_unit_stats_t;

/* Copy every battle unit that is not an egg, crystal or treasure into the next
 * status billboard record, store the shown-plus-egg unit count in each
 * record and sort the shown list; returns the number of shown units. `mode`
 * and `unit_data` are not read. */
s32 bunit_panel_build_unit_billboard_list(s32 mode, s32 unit_data, s32 sort_mode) {
    bunit_unit_stats_t* unit;
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
    do {
        unit = (bunit_unit_stats_t*)battle_unit_get_stats_from_battle_id(i);
        if ((unit != 0) && (unit->entd_slot != BATTLE_ENTD_SLOT_NONE)) {
            if (unit->unit_flags & UNIT_FLAG_EGG) {
                total++;
            } else if (!(unit->current_status_1 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL))
                && !(unit->current_status_2 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE))) {
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

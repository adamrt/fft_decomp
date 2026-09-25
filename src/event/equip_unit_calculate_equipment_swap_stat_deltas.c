#include "fft/event_equip.h"
#include "psx/types.h"

typedef struct equip_item_detail_delta_buffer {
    world_item_stat_detail_t detail;
    u8 _unused_40[0x40];
} equip_item_detail_delta_buffer_t;

/* Sum the stat changes of swapping each of the five equipment slots from `from_items` to `to_items`. */
void equip_unit_calculate_equipment_swap_stat_deltas(
    world_item_stat_detail_t* detail_total, world_item_stat_summary_t* summary_total, u16* from_items, u16* to_items) {
    world_item_stat_summary_t summary_delta;
    equip_item_detail_delta_buffer_t detail_delta;
    s32 i;

    /* The definition (equip_item_clear_stat_sums.c) writes summary[7] and summary[10] --
     * byte offsets 0x0e and 0x14, the world_item_stat_summary_t hp_bonus/mp_bonus
     * halfwords -- and detail[0..17] as a flat s16 stat block. So both
     * parameters are s16*, so the casts below preserve their flat views. */
    equip_item_clear_stat_sums((s16*)summary_total, (s16*)detail_total);

    for (i = 0; i < 5; i++) {
        equip_item_calculate_swap_stat_delta(
            &detail_delta.detail, &summary_delta, from_items[i] & 0x3FF, to_items[i] & 0x3FF, i);
        equip_item_subtract_scaled_stats(detail_total, detail_total, &detail_delta.detail, -1);
        summary_total->hp_bonus += summary_delta.hp_bonus;
        summary_total->mp_bonus += summary_delta.mp_bonus;
    }
}

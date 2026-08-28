#include "fft/world.h"
#include "psx/types.h"

/* Accumulates the stat deltas of all equipment slot changes into total. */
void world_item_calculate_equipment_swap_stat_delta(
    world_item_stat_detail_t* detail_total, world_item_stat_summary_t* total, u16* items_before, u16* items_after) {
    world_item_stat_summary_t delta;
    world_item_stat_detail_t detail;
    world_item_stat_detail_t unused;
    s32 slot;

    world_formation_clear_stat_preview(total, detail_total);
    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_COUNT; slot++) {
        world_item_calculate_swap_stat_delta(
            &detail, &delta, items_before[slot] & WORLD_ITEM_ID_MASK, items_after[slot] & WORLD_ITEM_ID_MASK, slot);
        world_item_combine_stat_details(detail_total, detail_total, &detail, -1);
        total->hp_bonus += delta.hp_bonus;
        total->mp_bonus += delta.mp_bonus;
    }
}

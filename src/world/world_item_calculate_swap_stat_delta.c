#include "fft/world.h"
#include "psx/types.h"

/* Compute the HP/MP and detailed stat changes for one equipment replacement. */
void world_item_calculate_swap_stat_delta(
    world_item_stat_detail_t* output, world_item_stat_summary_t* delta, s16 item_before, s16 item_after, s32 slot) {
    world_item_stat_summary_t summary_before;
    world_item_stat_summary_t summary_after;
    world_item_stat_detail_t detail_before;
    world_item_stat_detail_t detail_after;

    world_item_populate_stat_preview(item_before, &summary_before, &detail_before, slot);
    world_item_populate_stat_preview(item_after, &summary_after, &detail_after, slot);
    delta->hp_bonus = summary_after.hp_bonus - summary_before.hp_bonus;
    delta->mp_bonus = summary_after.mp_bonus - summary_before.mp_bonus;
    world_item_combine_stat_details(output, &detail_before, &detail_after, 1);
}

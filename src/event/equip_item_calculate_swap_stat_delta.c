#include "fft/equip.h"
#include "fft/world.h"
#include "psx/types.h"

/* Compute the HP/MP and detailed stat changes for one equipment replacement. */
void equip_item_calculate_swap_stat_delta(
    world_item_stat_detail_t* output, world_item_stat_summary_t* delta, s16 item_before, s16 item_after, s32 slot) {
    world_item_stat_summary_t summary_before;
    world_item_stat_summary_t summary_after;
    world_item_stat_detail_t detail_before;
    world_item_stat_detail_t detail_after;

    /* `item_before`/`item_after` are s16 formals that this caller sign-extends
     * at the call; the definition's s32 first parameter would drop that
     * extension. */
    ((void (*)(s16, world_item_stat_summary_t*, world_item_stat_detail_t*, s32))equip_collect_item_stat_deltas)(
        item_before, &summary_before, &detail_before, slot);
    ((void (*)(s16, world_item_stat_summary_t*, world_item_stat_detail_t*, s32))equip_collect_item_stat_deltas)(
        item_after, &summary_after, &detail_after, slot);
    delta->hp_bonus = summary_after.hp_bonus - summary_before.hp_bonus;
    delta->mp_bonus = summary_after.mp_bonus - summary_before.mp_bonus;
    equip_item_subtract_scaled_stats(output, &detail_before, &detail_after, 1);
}

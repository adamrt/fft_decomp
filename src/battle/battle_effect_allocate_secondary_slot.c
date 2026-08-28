/* Pop a slot from the free list and append it to the active list.
 *
 * The id is kept as an s32 byte load and a separate `slot = id & 0xff`
 * indexes the table: the target tests and indexes through the masked copy
 * ($v0) while storing and returning the raw id ($s0), which keeps the error
 * path's `move v0,s0` instead of a folded 0. Writing previous_slot_id before
 * the next_slot_id clear reproduces the target's early
 * g_battle_effect_active_slot_tail load.
 */
#include "fft/battle.h"
#include "fft/battle_effect.h"

s32 battle_effect_allocate_secondary_slot(void) {
    s32 id;
    s32 slot;
    s32 i;

    id = (u8)g_battle_effect_free_slot_head;
    slot = id & 0xff;
    if (slot != 0) {
        g_battle_effect_free_slot_head = g_battle_effect_secondary_data[slot].previous_slot_id;
        g_battle_effect_secondary_data[g_battle_effect_free_slot_head].next_slot_id = 0;
        g_battle_effect_secondary_data[slot].previous_slot_id = g_battle_effect_active_slot_tail;
        g_battle_effect_secondary_data[slot].next_slot_id = 0;
        g_battle_effect_secondary_data[g_battle_effect_active_slot_tail].next_slot_id = id;
        g_battle_effect_active_slot_tail = id;
        g_battle_effect_secondary_data[slot].timer = 0;
        for (i = 31; i >= 0; i--) {
            g_battle_effect_secondary_data[slot].slot_ids[i] = 0;
        }
        return id;
    }
    main_noop_800449f8(1, 0x66);
    return id;
}

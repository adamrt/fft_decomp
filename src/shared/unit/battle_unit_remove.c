#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/*
 * Unlink a runtime unit record from the list and release its per-unit slot.
 *
 * The spritesheet VRAM slot is released only when no other live unit shares it.
 */
void battle_unit_remove(battle_unit_t* unit) {
    battle_unit_t* it;
    s32 shared;

    shared = 0;
    battle_gfx_release_unit_sprite_display_stub(unit->animation.display);
    g_battle_unit_slot_states[unit->runtime_id].in_use = 0;
    /* The 32-bit list-head pointer doubles as a sentinel record (previous is at 0x000);
     * reading the first link through that view keeps the target's addressing. */
    for (it = ((battle_unit_t*)&g_battle_unit_list_head)->previous; it != 0; it = it->previous) {
        if (it->spritesheet_vram_slot == unit->spritesheet_vram_slot
            && g_battle_unit_slot_states[it->runtime_id].in_use != 0) {
            shared = 1;
        }
    }
    if (shared == 0) {
        g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].in_use = 0;
    }
    for (it = (battle_unit_t*)&g_battle_unit_list_head; it != 0; it = it->previous) {
        if (it->previous == unit) {
            it->previous = unit->previous;
            return;
        }
    }
}

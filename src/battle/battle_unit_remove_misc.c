#include "fft/battle.h"

/*
 * Unlink a Misc unit_t record from the list and release its per-unit slot.
 *
 * The spritesheet VRAM slot is released only when no other live unit shares it.
 */
void battle_unit_remove_misc(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* it;
    s32 shared;

    shared = 0;
    /* The target passes the display section to the argument-less no-op. */
    ((void (*)(void*))battle_noop_8007aecc)(unit->sprite_display_section);
    g_battle_unit_misc_slot_flags[unit->unit_id].in_use = 0;
    /* The list-head word doubles as a sentinel record (previous is at 0x000);
     * reading the first link through that view keeps the target's addressing. */
    for (it = ((battle_unit_misc_data_t*)&g_battle_unit_misc_list_head)->previous; it != 0; it = it->previous) {
        if (it->spritesheet_vram_slot == unit->spritesheet_vram_slot
            && g_battle_unit_misc_slot_flags[it->unit_id].in_use != 0) {
            shared = 1;
        }
    }
    if (shared == 0) {
        g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].in_use = 0;
    }
    for (it = (battle_unit_misc_data_t*)&g_battle_unit_misc_list_head; it != 0; it = it->previous) {
        if (it->previous == unit) {
            it->previous = unit->previous;
            return;
        }
    }
}

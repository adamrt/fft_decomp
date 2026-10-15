#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

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

void battle_unit_clear_all(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_remove(unit);
        unit = unit->previous;
    }
}

/* Steps a unit's fade-out removal; each phase waits for the unit's palette
 * animation to end.
 *
 * FLASH starts the fade to black and moves to DARKEN. DARKEN then disables the
 * battle unit slot, removes the runtime record and returns 1; for the source unit it
 * only resets the graphic trigger. Every other path returns 0. */
s32 battle_unit_update_fade_out_removal(battle_unit_t* unit) {
    u8 phase;

    phase = unit->fade_out_phase;
    if (phase == BATTLE_UNIT_FADE_OUT_NONE) {
        return 0;
    }
    if (g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].animations[unit->runtime_id].active != 0) {
        return 0;
    }
    switch (phase) {
    case BATTLE_UNIT_FADE_OUT_FLASH:
        unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_DARKEN;
        battle_gfx_apply_unit_palette_modulation_to_units(
            MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -0x1f, -0x1f, -0x1f);
        return 0;
    case BATTLE_UNIT_FADE_OUT_DARKEN:
        if (unit->runtime_id != g_battle_unit_source_runtime_id) {
            unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_REMOVED;
            if (unit->stats != 0) {
                battle_unit_disable_remove(unit->stats->battle_id);
            }
            battle_unit_remove(unit);
            return 1;
        }
        battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
        return 0;
    }
    return 0;
}

/* Per-frame update of every unit record: its animation with the weapon and effect
 * sprites, its status bubble and its fade-out removal. */
void battle_unit_update_and_animate_units(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_gfx_update_and_animate_unit_wep_eff(unit);
        battle_gfx_init_status_bubble(unit);
        battle_unit_update_fade_out_removal(unit);
        unit = unit->previous;
    }
}

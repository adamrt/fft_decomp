#include "fft/battle.h"
#include "psx/types.h"

/* Spawn this frame's share of a trap group's effect slots.
 *
 * The group record's byte 0x28 caps how many slots one frame may start; the
 * secondary effect's active_count counts the slots started so far and stops at the
 * group's total count. */
void battle_effect_start_group(s32 group, s32 animation) {
    battle_effect_secondary_data_t* effect;
    battle_effect_slot_t* slot;
    s32 spawned;
    s32 i;
    u8 id;

    if (g_battle_effect_groups[group].count > g_battle_effect_current_secondary->active_count
        && g_battle_effect_current_secondary->timer >= g_battle_effect_groups[group].spawn_start_frame
        && g_battle_effect_current_secondary->timer < g_battle_effect_groups[group].spawn_end_frame) {
        for (spawned = 0; spawned < g_battle_effect_groups[group].spawns_per_frame; spawned++) {
            for (i = 0; i < g_battle_effect_groups[group].count; i++) {
                if (g_battle_effect_current_secondary->slot_ids[i] == 0) {
                    break;
                }
            }
            id = battle_effect_alloc_slot();
            g_battle_effect_current_secondary->slot_ids[i] = id;
            if (id == 0) {
                break;
            }
            slot = &g_battle_effect_slots[g_battle_effect_current_secondary->slot_ids[i]];
            slot->life = battle_effect_spawn_particle_motion(
                (battle_effect_emitter_values_t*)(&g_battle_effect_trap_particle_configs[group * 0x2e]), &slot->motion);
            battle_effect_init_trap_animation(group, animation + 0x7ac0, slot);
            effect = g_battle_effect_current_secondary;
            effect->active_count++;
            if (effect->active_count == g_battle_effect_groups[group].count) {
                break;
            }
        }
    }
}

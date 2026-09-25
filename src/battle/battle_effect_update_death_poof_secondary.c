#include "fft/battle.h"
#include "psx/types.h"

/* Provisional eight-byte per-secondary-effect spawn state at 0x801bc0e0,
 * indexed by battle_effect_secondary_data_t::own_slot_id. It carries the same
 * roles the generic trap spawner keeps in the effect record itself. */
typedef struct battle_effect_trap_state {
    s32 count;    /* 0x00; slots started so far */
    u16 field_04; /* 0x04; reaches 1 when the effect is finished */
    u8 ids[2];    /* 0x06 */
} battle_effect_trap_state_t;

extern battle_effect_trap_state_t g_battle_effect_death_poof_states[];

enum {
    BATTLE_EFFECT_TRAP_GROUP = 4,
    BATTLE_EFFECT_TRAP_SLOT_COUNT = 2,
    BATTLE_EFFECT_TRAP_ANIMATION = 0x7acf,
};

/*
 * Death Poof secondary effect (0x0f) handler for the two-slot trap group 4: it
 * starts this frame's share of the group's effect slots, steps them with a
 * (0, 0x1000, 0) on-hit vector and finishes once the spawn state's field_04
 * reaches 1.
 *
 * The unrecognized-phase return path preserves its second incoming argument
 * (`passthrough`).
 * The handler table calls through a no-argument function-pointer type, so the
 * arguments remain provisional ABI evidence rather than semantic parameters.
 */
s32 battle_effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough) {
    battle_effect_secondary_data_t* effect;
    battle_effect_trap_state_t* state;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    battle_effect_on_hit_vector_t on_hit_vector;
    s32 result;
    s32 spawned;
    s32 i;

    effect = g_battle_effect_current_secondary;
    result = passthrough;
    state = &g_battle_effect_death_poof_states[effect->own_slot_id];
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING: {
        state->count = 0;
        for (i = 0; i < BATTLE_EFFECT_TRAP_SLOT_COUNT; i++) {
            state->ids[i] = 0;
        }
        state->field_04 = 0;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        result = 1;
        break;
    }
    case BATTLE_SECONDARY_EFFECT_EXECUTING: {
        battle_effect_slot_t* slot;
        s32 i;
        u8 id;

        if (effect->timer >= g_battle_effect_groups[BATTLE_EFFECT_TRAP_GROUP].spawn_start_frame
            && effect->timer < g_battle_effect_groups[BATTLE_EFFECT_TRAP_GROUP].spawn_end_frame) {
            for (spawned = 0; spawned < g_battle_effect_groups[BATTLE_EFFECT_TRAP_GROUP].spawns_per_frame; spawned++) {
                for (i = 0; i < BATTLE_EFFECT_TRAP_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = battle_effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_battle_effect_slots[state->ids[i]];
                slot->life = battle_effect_spawn_particle_motion(
                    (battle_effect_emitter_values_t*)((u8*)&g_battle_effect_groups[BATTLE_EFFECT_TRAP_GROUP] - 2),
                    &slot->motion);
                battle_effect_init_trap_animation(BATTLE_EFFECT_TRAP_GROUP, BATTLE_EFFECT_TRAP_ANIMATION, slot);
                state->count++;
                if (state->count == BATTLE_EFFECT_TRAP_SLOT_COUNT) {
                    break;
                }
            }
        }
    }
    /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING: {
        battle_effect_secondary_data_t* current;
        u8 id;

        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        on_hit_vector.z = 0;
        on_hit_vector.x = 0;
        on_hit_vector.y = ONE;
        battle_effect_copy_on_hit_data_to_second_section();
        battle_effect_store_first_section_of_on_hit_data(&on_hit_vector);
        for (i = 0; i < BATTLE_EFFECT_TRAP_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_copy_second_section_to_on_hit_data();
        current = g_battle_effect_current_secondary;
        current->timer = current->timer + 1;
        result = state->field_04 != 1;
        if (state->count == 0
            && (s16)current->timer > (s32)g_battle_effect_groups[BATTLE_EFFECT_TRAP_GROUP].spawn_start_frame) {
            state->field_04 = state->field_04 + 1;
        }
        break;
    }
    }
    return result;
}

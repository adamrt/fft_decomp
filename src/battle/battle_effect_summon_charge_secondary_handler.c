#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/effect_summon_mesh.h"
#include "fft/main_sound.h"
#include "psx/gte.h"
#include "psx/types.h"

enum {
    BATTLE_EFFECT_RING_GROUP = 14,
    BATTLE_EFFECT_RING_ANIMATION = 0x7acc,
    BATTLE_EFFECT_RING_SLOT_COUNT = 30,
};

/* Secondary effect 0x16 (summon charge): three rings of ten effect slots
 * spinning around the target.
 *
 * Listed as "Smn chrg" in the secondary handler table at 0x801b8900.
 * Initialisation allocates the 30
 * slots and plays sound 0x21; the executing phase grows the radius, spin and
 * brightness with the timer, the finalizing phase widens the rings and fades
 * them out over finish_timer, and both then advance the ring angle, record each
 * ring's newest point and place every slot at a trailing point. Once finish_timer
 * passes 60 the slots are freed and sound 0x21 is paused. Returns 1 while the
 * effect runs.
 */
s32 battle_effect_summon_charge_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    battle_effect_ring_state_t* state;
    battle_effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_battle_effect_current_secondary;
    state = &g_battle_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        battle_effect_resolve_target_render_coords(0, -1, &g_battle_effect_coord_data, &state->center);
        state->brightness = 0;
        state->center.vy -= 0x18;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < BATTLE_EFFECT_RING_SLOT_COUNT; i++) {
            j = g_battle_effect_current_secondary->slot_ids[i] = battle_effect_alloc_slot();
            if (j != 0) {
                slot = &g_battle_effect_slots[j];
                ((battle_effect_slot_t*)slot)->life = battle_effect_spawn_particle_motion(
                    (battle_effect_emitter_values_t*)((u8*)&g_battle_effect_groups[BATTLE_EFFECT_RING_GROUP] - 2),
                    &slot->motion);
                battle_effect_init_trap_animation(
                    BATTLE_EFFECT_RING_GROUP, BATTLE_EFFECT_RING_ANIMATION, (battle_effect_slot_t*)slot);
            }
        }
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->finish_timer = 0;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        SuzukiPlaySoundFindChannel(0x21);
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        if (effect->timer < 0x18) {
            state->radius = effect->timer;
            state->angle_step = effect->timer * 3;
        }
        if (effect->timer < 8) {
            state->brightness = (effect->timer + 1) * 16;
        }
        if (effect->timer < 0x100) {
            effect->timer++;
        }
        goto update; /* skip the finalizing fade into the shared update */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        if (effect->finish_timer < 0x3d) {
            if (effect->finish_timer & 8) {
                state->radius++;
            }
            state->angle_step++;
            if (effect->finish_timer < 0x3d) {
                state->brightness = (0x3c - effect->finish_timer) * 4;
                if (effect->finish_timer < 0x3d) {
                    effect->finish_timer++;
                }
            }
        }
    update:
        state->angle += state->angle_step;
        for (i = 0; i < 3; i++) {
            state->points[i][state->index].vx = rcos(i * 0x555 + state->angle) * state->radius / ONE;
            state->points[i][state->index].vz = rsin(i * 0x555 + state->angle) * state->radius / ONE;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                id = g_battle_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_battle_effect_slots[id];
                    slot->red = slot->green = slot->blue
                        = g_battle_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_battle_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_battle_effect_slots[id];
                    slot->motion.position[0] = (state->center.vx + state->points[i][k].vx) << 12;
                    slot->motion.position[1] = (state->center.vy + state->points[i][k].vy) << 12;
                    slot->motion.position[2] = (state->center.vz + state->points[i][k].vz) << 12;
                    battle_effect_update_slot(g_battle_effect_current_secondary->slot_ids[i * 10 + j]);
                }
                k++;
                if (k == 10) {
                    k = 0;
                }
            }
        }
        state->index++;
        if (state->index >= 10) {
            state->index = 0;
        }
        /* The finished arm comes first. Its jump and barrier then precede the
         * result = 1 block, so reorg's liveness at the return label no longer
         * sees a0 from the final call's delay slot and fills the phase-1
         * branch slot at the dispatch as the target does. */
        if (g_battle_effect_current_secondary->finish_timer >= 0x3d) {
            for (i = 0; i < BATTLE_EFFECT_RING_SLOT_COUNT; i++) {
                if (g_battle_effect_current_secondary->slot_ids[i] != 0) {
                    battle_effect_free_slot(g_battle_effect_current_secondary->slot_ids[i]);
                    g_battle_effect_current_secondary->slot_ids[i] = 0;
                }
            }
            main_sound_stop_sfx_channels(0x21);
            result = 0;
        } else {
            result = 1;
        }
        break;
    }
    return result;
}

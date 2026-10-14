#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/main.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

enum {
    EFFECT_RING_CLUT = MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 12),
    EFFECT_RING_SLOT_COUNT = 30,
};

/* Secondary effect 0x16 (summon charge): three rings of ten effect slots
 * spinning around the target.
 *
 * Initialization allocates the 30
 * slots and plays sound 0x21; the executing phase grows the radius, spin and
 * brightness with the timer, the finalizing phase widens the rings and fades
 * them out over finish_timer, and both then advance the ring angle, record each
 * ring's newest point and place every slot at a trailing point. Once finish_timer
 * passes 60 the slots are freed and sound 0x21 is paused. Returns 1 while the
 * effect runs.
 */
s32 effect_summon_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_ring_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &state->center);
        state->brightness = 0;
        state->center.vy -= 24;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
            j = g_effect_current_secondary->slot_ids[i] = effect_alloc_slot();
            if (j != 0) {
                slot = &g_effect_slots[j];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                effect_init_trap_animation(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
            }
        }
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_SUMMON_CHARGE);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (effect->timer < 24) {
            state->radius = effect->timer;
            state->angle_step = effect->timer * 3;
        }
        if (effect->timer < 8) {
            state->brightness = (effect->timer + 1) * 16;
        }
        if (effect->timer < 256) {
            effect->timer++;
        }
        goto update; /* skip the finalizing fade into the shared update */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        if (effect->finish_timer < 61) {
            if (effect->finish_timer & 8) {
                state->radius++;
            }
            state->angle_step++;
            if (effect->finish_timer < 61) {
                state->brightness = (60 - effect->finish_timer) * 4;
                if (effect->finish_timer < 61) {
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
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->color.r = slot->color.g = slot->color.b
                        = g_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->motion.position.x = (state->center.vx + state->points[i][k].vx) * ONE;
                    slot->motion.position.y = (state->center.vy + state->points[i][k].vy) * ONE;
                    slot->motion.position.z = (state->center.vz + state->points[i][k].vz) * ONE;
                    effect_update_slot(g_effect_current_secondary->slot_ids[i * 10 + j]);
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
        if (g_effect_current_secondary->finish_timer >= 61) {
            for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
                if (g_effect_current_secondary->slot_ids[i] != 0) {
                    effect_free_slot(g_effect_current_secondary->slot_ids[i]);
                    g_effect_current_secondary->slot_ids[i] = 0;
                }
            }
            suzuki_sound_stop_sfx_channels(MAIN_SFX_SUMMON_CHARGE);
            result = 0;
        } else {
            result = 1;
        }
        break;
    }
    return result;
}

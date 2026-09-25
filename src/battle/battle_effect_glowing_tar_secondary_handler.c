#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/main_heap.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Local view of battle_effect_secondary_data_t::target.fields including the
 * result-animation byte copied from the init record's +0x07. */
typedef struct battle_effect_secondary_target_fields {
    s16 target_type;
    u8 target_id;        /* 0x1c */
    u8 result_animation; /* 0x1d */
} battle_effect_secondary_target_fields_t;

/* Provisional 0x28-byte heap state kept in the secondary effect's allocation. */
typedef struct battle_effect_burst_state {
    s32 frame;    /* 0x00 */
    s32 done;     /* 0x04 */
    s32 count_a;  /* 0x08 */
    u8 ids_a[8];  /* 0x0c; group 0 slots */
    s32 count_b;  /* 0x14 */
    u8 ids_b[16]; /* 0x18; group 1 or 9 slots */
} battle_effect_burst_state_t;

s32 battle_effect_glowing_tar_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    battle_effect_burst_state_t* state;
    battle_effect_slot_t* slot;
    s32 unused[8];
    battle_effect_on_hit_vector_t pos;
    VECTOR zoom;
    VECTOR offset;
    s32 result;
    s32 group;
    s32 j;
    s32 done;
    s32 i;
    u8 id;

    effect = g_battle_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        if (((battle_effect_secondary_target_fields_t*)&effect->target)->result_animation == 4) {
            return 0;
        }
        if (effect->allocation != 0) {
            main_heap_free(effect->allocation);
        }
        state = game_malloc(sizeof(battle_effect_burst_state_t));
        g_battle_effect_current_secondary->allocation = state;
        state->frame = 0;
        state->done = 0;
        state->count_a = 0;
        state->count_b = 0;
        for (i = 0; i < 8; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 16; i++) {
            state->ids_b[i] = 0;
        }
        result = 1;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        battle_effect_copy_camera_angles_and_screen_coords();
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        state = effect->allocation;
        group = 1;
        if (((battle_effect_secondary_target_fields_t*)&effect->target)->result_animation == 3) {
            group = 9;
        }
        if (group == 1) {
            if (state->frame == 0) {
                battle_gfx_start_misc_unit_palette_modulation(
                    5, 0, ((battle_effect_secondary_target_fields_t*)&effect->target)->target_id, 0x1f, 0x1f, 0x1f);
            }
            if (state->frame == group) {
                battle_gfx_start_misc_unit_palette_modulation(8, 2,
                    ((battle_effect_secondary_target_fields_t*)&g_battle_effect_current_secondary->target)->target_id,
                    0, 0, 0);
            }
        }
        if (state->frame < 4) {
            if (state->count_a < 8) {
                for (i = 0; i < g_battle_effect_groups[0].spawns_per_frame; i++) {
                    for (j = 0; j < 8; j++) {
                        if (state->ids_a[j] == 0) {
                            break;
                        }
                    }
                    state->ids_a[j] = battle_effect_alloc_slot();
                    if (state->ids_a[j] == 0) {
                        break;
                    }
                    slot = &g_battle_effect_slots[state->ids_a[j]];
                    slot->life = battle_effect_spawn_particle_motion(
                        (battle_effect_emitter_values_t*)((u8*)&g_battle_effect_groups[0] - 2), &slot->motion);
                    battle_effect_init_trap_animation(0, g_battle_effect_current_secondary->parameter + 0x7ac0, slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }
            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_battle_effect_groups[group].spawns_per_frame; i++) {
                    for (j = 0; j < 16; j++) {
                        if (state->ids_b[j] == 0) {
                            break;
                        }
                    }
                    state->ids_b[j] = battle_effect_alloc_slot();
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                    slot = &g_battle_effect_slots[state->ids_b[j]];
                    slot->life = battle_effect_spawn_particle_motion(
                        (battle_effect_emitter_values_t*)((u8*)&g_battle_effect_groups[group] - 2), &slot->motion);
                    battle_effect_init_trap_animation(1, 0x7aca, slot);
                    slot->red = slot->green = slot->blue = 0xff;
                    state->count_b++;
                    if (state->count_b == 16) {
                        break;
                    }
                }
            }
        }
        /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        pos.z = 0;
        pos.x = 0;
        pos.y = ONE;
        battle_effect_copy_on_hit_data_to_second_section();
        battle_effect_store_first_section_of_on_hit_data(&pos);
        for (i = 0; i < 8; i++) {
            id = state->ids_a[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_a[i] = 0;
                state->count_a--;
            }
        }
        for (i = 0; i < 16; i++) {
            id = state->ids_b[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_b[i] = 0;
                state->count_b--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_copy_second_section_to_on_hit_data();
        if (((battle_effect_secondary_target_fields_t*)&g_battle_effect_current_secondary->target)->result_animation
            == 1) {
            if (state->frame == 0) {
                zoom.vx = zoom.vy = zoom.vz = -0x80;
                battle_camera_execute_zoom_command(0x500, 0, 2, &zoom);
            }
            if (state->frame == 2) {
                zoom.vx = zoom.vy = zoom.vz = 0x2e4;
                battle_camera_execute_zoom_command(0x500, 0, 2, &zoom);
            }
            if (state->frame == 4) {
                zoom.vx = zoom.vy = zoom.vz = 0;
                battle_camera_execute_zoom_command(0x500, 0, 0xc, &zoom);
                offset.vx = offset.vy = offset.vz = 4;
                battle_camera_execute_position_command(0x1440, 0, 9, &offset);
            }
        }
        done = state->done;
        result = done != 1;
        state->frame++;
        if (state->count_a == 0 && state->count_b == 0 && state->frame >= 0x1f) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

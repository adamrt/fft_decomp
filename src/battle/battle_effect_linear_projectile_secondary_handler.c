#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/gte.h"

/* 0x2a0-byte work buffer: two sets of twelve G3 polygons. */
typedef struct battle_effect_projectile_g3x12_work {
    battle_effect_g3_prim_t g3[2][12];
} battle_effect_projectile_g3x12_work_t;

/* 0x380-byte work buffer: two sets of sixteen G3 polygons. */
typedef struct battle_effect_projectile_g3x16_work {
    battle_effect_g3_prim_t g3[2][16];
} battle_effect_projectile_g3x16_work_t;

/* Effect slot view: the double-buffered item sprite quad at 0x34. */
typedef struct battle_effect_item_slot_view {
    u8 _unknown_00[0x34];
    POLY_FT4 prims[2]; /* 0x34 */
} battle_effect_item_slot_view_t;

s32 battle_effect_linear_projectile_secondary_handler(void) {
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    battle_effect_rotation_vector_t angles;
    VECTOR scale;
    battle_effect_rotation_vector_t spin;
    SPRT sprite;
    battle_effect_arrow_buffer_t* arrow;
    battle_effect_projectile_g3x12_work_t* work12;
    battle_effect_projectile_g3x16_work_t* work16;
    battle_effect_item_slot_view_t* slot;
    s32 parameter;
    s32 item;
    s32 i;
    s32 j;
    s32 result;
    s32 kind;
    POLY_FT4* dst;

    switch ((u32)g_battle_effect_current_secondary->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
        delta.vy = g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy;
        delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
        g_battle_effect_projectile_length
            = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) << 12);
        g_battle_effect_projectile_delta = delta;
        g_battle_effect_projectile_progress = 0;
        g_battle_effect_projectile_speed = 0x8000;
        g_battle_effect_projectile_spin_passes = g_battle_effect_trajectory_step_count;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        if ((parameter & 0xff) == 0) {
            arrow = main_heap_alloc(0xc88);
            g_battle_effect_current_secondary->allocation = arrow;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 4; j++) {
                    SetPolyG3(&arrow->g3[i][j]);
                }
                for (j = 0; j < 13; j++) {
                    SetPolyG4(&arrow->g4[i][j]);
                }
            }
        }
        if ((parameter & 0xff) == 1) {
            work12 = main_heap_alloc(0x2a0);
            g_battle_effect_current_secondary->allocation = work12;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&work12->g3[i][j]);
                }
            }
        }
        if (parameter == -1) {
            item = g_battle_effect_current_secondary->used_weapon_id;
            if ((u32)(item - 0x7a) < 3) {
                work16 = main_heap_alloc(0x380);
                g_battle_effect_current_secondary->allocation = work16;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&work16->g3[i][j]);
                    }
                }
            } else {
                battle_get_item_graphic_data(&sprite, item);
                g_battle_effect_current_secondary->slot_ids[0] = battle_effect_alloc_slot();
                slot = (battle_effect_item_slot_view_t*)&g_battle_effect_slots[g_battle_effect_current_secondary
                        ->slot_ids[0]];
                dst = &slot->prims[1];
                SetPolyFT4(&slot->prims[0]);
                slot->prims[0].tpage = 0x1e;
                slot->prims[0].b0 = 0x80;
                slot->prims[0].g0 = 0x80;
                slot->prims[0].r0 = 0x80;
                slot->prims[0].u0 = slot->prims[0].u2 = sprite.u0;
                slot->prims[0].u1 = slot->prims[0].u3 = slot->prims[0].u0 + sprite.w;
                slot->prims[0].v0 = slot->prims[0].v1 = sprite.v0;
                slot->prims[0].v2 = slot->prims[0].v3 = slot->prims[0].v0 + sprite.h;
                slot->prims[0].clut = sprite.clut;
                *dst = slot->prims[0];
            }
        }
        battle_state_set_animation_speed(1);
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        g_battle_effect_projectile_progress += g_battle_effect_projectile_speed;
        battle_effect_find_projectile_xyz_along_trajectory(&g_battle_effect_projectile_delta,
            g_battle_effect_projectile_length, g_battle_effect_projectile_progress, &position);
        position.vx = g_battle_effect_trajectory_origin.vx + position.vx;
        position.vy = g_battle_effect_trajectory_origin.vy + position.vy;
        position.vz = g_battle_effect_trajectory_origin.vz + position.vz;
        delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
        delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
        delta.vy = g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy;
        angles.y = ratan2(-delta.vz, delta.vx);
        angles.z = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.x = 0;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        i = parameter & 0xff;
        if (parameter != -1) {
            if (i == 0) {
                scale.vz = 0x40;
                scale.vy = 0x40;
                scale.vx = 0x40;
                spin.z = 0;
                spin.y = 0;
                spin.x = 0;
            } else if (i == 1) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.x = 0;
                spin.y = g_battle_effect_projectile_spin_passes * 256;
                spin.z = g_battle_effect_projectile_spin_passes * 128;
            }
            battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        } else {
            item = g_battle_effect_current_secondary->used_weapon_id;
            if ((u32)(item - 0x7a) < 3) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.y = 0;
                spin.x = 0;
                spin.z = -(g_battle_effect_projectile_spin_passes * 256);
                battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, 3);
            } else {
                battle_effect_draw_item_drop_sprite(&position, item, 0);
            }
        }
        if (g_battle_effect_projectile_spin_passes == 2 && g_battle_effect_trajectory_hit_unit_id != -1) {
            kind = g_battle_effect_current_secondary->used_weapon_id;
            if ((u32)(kind - 0x7d) >= 3 && (u32)(kind - 0xf0) >= 0xe
                && g_battle_effect_current_secondary->target_count != 0) {
                battle_unit_call_bow_hardcoding_by_misc_id(
                    (u8)g_battle_effect_current_secondary->caster_block.values[1],
                    g_battle_effect_current_secondary->target.fields.target_id);
            }
        }
        if (g_battle_effect_projectile_spin_passes > 0) {
            g_battle_effect_projectile_spin_passes--;
        }
        if (g_battle_effect_projectile_spin_passes == 0) {
            if (g_battle_effect_current_secondary->slot_ids[0] != 0) {
                battle_effect_free_slot(g_battle_effect_current_secondary->slot_ids[0]);
                g_battle_effect_current_secondary->slot_ids[0] = 0;
            }
            if (*(u16*)&g_battle_effect_current_secondary->target.fields.target_type == 0
                && (u32)(g_battle_effect_current_secondary->target.fields.target_byte_1d - 3) >= 2) {
                battle_effect_play();
                battle_state_set_animation_speed(2);
            } else if (battle_effect_get_phase() == 1) {
                if (g_battle_effect_current_secondary->target_count != 0) {
                    battle_unit_call_bow_hardcoding_by_misc_id(
                        (u8)g_battle_effect_current_secondary->caster_block.values[1],
                        g_battle_effect_current_secondary->target.fields.target_id);
                }
                battle_effect_set_phase_none();
            }
            g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_FINALIZING;
        }
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

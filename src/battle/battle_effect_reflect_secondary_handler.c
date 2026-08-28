#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/effect.h"
#include "fft/main_heap.h"
#include "psx/gpu.h"
#include "psx/gte.h"

/* 0x28-byte GT3 polygon initialised by SetPolyGT3. */
typedef struct battle_effect_reflect_prim {
    u8 data[0x28];
} battle_effect_reflect_prim_t;

/* 0x798-byte work buffer: two 6-entry CLUT rows then two 24-polygon strips. */
typedef struct battle_effect_reflect_work {
    u16 clut[2][6];
    battle_effect_reflect_prim_t prims[2][24];
} battle_effect_reflect_work_t;

/* Secondary-effect handler 0x12 (Reflect).
 *
 * Phase 1 allocates and initialises the work buffer; phase 2 advances a
 * two-entry CLUT wipe near the end of the effect, uploads the CLUT, aims the
 * shield from the target's centre toward the caster and hands the transform
 * to 0x801ae340; phase 3 finishes. The wipe indices are assigned inside the
 * subscript, each to its own variable: a separate statement or a folded
 * `timer - 1 - span` changes the order and association of the index
 * arithmetic. spin_timer keeps the timer load at full width (lh). */
s32 battle_effect_reflect_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    battle_effect_rotation_vector_t angles;
    VECTOR scale;
    battle_effect_rotation_vector_t spin;
    RECT rect;
    battle_effect_reflect_work_t* work;
    battle_screen_coords_t* caster;
    battle_screen_coords_t* target;
    u8* rgb;
    u8* red;
    u8* green;
    u8* blue;
    s32 span;
    s32 timer;
    s32 lit;
    s32 cleared;
    s32 spin_timer;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;

    span = (u16)g_battle_effect_current_secondary->parameter;
    if (span < 0x1a) {
        span = 0x1a;
    }
    switch ((u32)g_battle_effect_current_secondary->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        work = game_malloc(0x798);
        g_battle_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 0x18; j++) {
                SetPolyGT3(&work->prims[i][j]);
                SetSemiTrans(&work->prims[i][j], 1);
            }
        }
        for (i = 0; i < 2; i++) {
            rgb = g_battle_effect_reflect_palette_colors;
            blue = rgb;
            green = rgb + 1;
            red = rgb + 2;
            for (j = 0; j < 6; j++) {
                work->clut[i][j] = blue[j * 3] | (green[j * 3] << 5) | (red[j * 3] << 10) | 0x8000;
            }
        }
        result = 1;
        g_battle_effect_current_secondary->timer = 0;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        work = g_battle_effect_current_secondary->allocation;
        timer = g_battle_effect_current_secondary->timer;
        if (span < timer && timer < span + 4) {
            work->clut[g_battle_effect_buffer_index][((lit = timer - 1) - span) * 2]
                = work->clut[g_battle_effect_buffer_index][(lit - span) * 2 + 1] = 0xffff;
        }
        if (span + 1 < g_battle_effect_current_secondary->timer) {
            work->clut[g_battle_effect_buffer_index][((cleared = g_battle_effect_current_secondary->timer - 2) - span)
                * 2] = work->clut[g_battle_effect_buffer_index][(cleared - span) * 2 + 1] = 0;
        }
        rect.x = (g_battle_effect_current_secondary->own_slot_id - 1) * 16 + 1;
        rect.y = 0x1fc;
        rect.w = 6;
        rect.h = 1;
        LoadImage(&rect, (u32*)work->clut[g_battle_effect_buffer_index]);
        caster
            = battle_unit_get_screen_data_ptr_by_misc_id((u8)g_battle_effect_current_secondary->caster_block.values[1]);
        target = battle_unit_get_screen_data_ptr_by_misc_id(g_battle_effect_current_secondary->target.fields.target_id);
        delta.vx = caster->x - target->x;
        delta.vy = caster->z - target->z;
        delta.vz = caster->y - target->y;
        position.vx = target->x;
        position.vz = target->y;
        position.vy = target->z
            - (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(
                  g_battle_effect_current_secondary->target.fields.target_id)
                / 2;
        angles.y = ratan2(-delta.vz, delta.vx);
        angles.z = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.x = 0;
        scale.vz = 0x140;
        scale.vy = 0x140;
        scale.vx = 0x140;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        spin.z = 0;
        spin.x = 0;
        spin_timer = g_battle_effect_current_secondary->timer;
        spin.z = spin_timer << 7;
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        effect = g_battle_effect_current_secondary;
        effect->timer++;
        if (span + 4 < effect->timer) {
            effect->phase = BATTLE_SECONDARY_EFFECT_FINALIZING;
        }
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

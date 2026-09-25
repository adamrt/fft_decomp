#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Header words of the unit SEQ data at misc unit data 0x1f8. */
typedef struct battle_unit_seq_header {
    u32 first_animation; /* 0x00 */
    u32 animation_count; /* 0x04 */
} battle_unit_seq_header_t;

/* Update one unit's sprite animation for the frame.
 *
 * The facing relative to the camera yaw selects a quadrant and a sixteenth of
 * a turn. A requested animation 3 restarts the walk cycle for that sixteenth;
 * other requests (throttled to one unit in four per frame below 12) build the
 * encoded animation from the request and quadrant, loading the unit into an
 * EVTCHR VRAM slot (evicting the slot 1 owner if needed) when the SEQ data
 * keeps the animation there. Without a request a change of sixteenth refreshes
 * the walk or idle pose; otherwise the animation and the three weapon/effect
 * sprite countdowns advance by g_animation_speed frames.
 *
 * unit is initialized in its declaration so its copy precedes the table
 * copies, and the unread unused_10 sizes the target's frame. */
void battle_gfx_update_and_animate_unit_wep_eff(battle_unit_misc_data_t* unit_data) {
    battle_unit_misc_data_t* unit = unit_data;
    s16 unused_10[4]; /* Unread; occupies sp+0x10 like the target. */
    s16 flip_by_quadrant[4] = { 0, 0, 2, 2 };
    s16 animation_by_quadrant[4] = { 0, 1, 1, 0 };
    s16 flip_by_direction[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0 };
    s16 walk_by_direction[16] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 3, 3, 2, 2, 1 };
    battle_unit_misc_data_t* other;
    battle_unit_seq_header_t* seq;
    s32 direction;
    s16 quadrant;
    s16 sixteenth;
    s32 animation;
    s32 index;
    s32 ok;
    u16 encoded;
    s32 i;
    s32 j;

    if (unit->ability_in_use == 0) {
        return;
    }
    if (unit->position_copies_active != 0) {
        battle_gfx_shift_and_fill_display_svectors((battle_gfx_render_unit_t*)unit);
    }
    direction = (g_battle_camera_render_state.vy + (s16)unit->facing) & 0xfff;
    quadrant = direction / 1024;
    sixteenth = direction / 256;
    if (unit->requested_animation != 0) {
        if (unit->requested_animation - 1 == 2) {
            for (i = 0; i < 3; i++) {
                unit->sprite_blocks[i].trigger = 0;
            }
            unit->encoded_animation = walk_by_direction[sixteenth];
            *(u16*)&unit->sprite_display_flags
                = (*(u16*)&unit->sprite_display_flags & 0xfff9) | flip_by_direction[sixteenth];
            unit->camera_facing_quadrant.s = quadrant;
            unit->camera_facing_sixteenth.s = sixteenth;
            unit->animation_script_pos = 0;
            unit->animation_countdown = 0;
            unit->secondary_animation_state = 0;
            unit->effect_vector.vz = 0;
            unit->effect_vector.vy = 0;
            unit->effect_vector.vx = 0;
            unit->screen_offset.vz = 0;
            unit->screen_offset.vy = 0;
            unit->screen_offset.vx = 0;
            unit->animation_wait_bias = 0;
            unit->animation_frame_bias = 0;
            unit->animation_loop_count = 0;
            unit->animation_flags = 0;
            battle_gfx_run_unit_seq_script(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 1);
            unit->requested_animation = 0;
            battle_gfx_release_unit_vram_slot((u8*)unit);
        } else if (unit->requested_animation >= 12 || (unit->unit_id & 3) == g_battle_frame_counter % 4
            || (unit->movement.word & 0x20000000)) {
            /* Units below animation 12 update only on their frame slot. */
            ok = 1;
            if ((unit->requested_animation - 1) * 2 + animation_by_quadrant[quadrant] != unit->encoded_animation
                || unit->animation_countdown == 0 || (unit->movement.word & 0x20000000)) {
                animation = unit->requested_animation - 1;
                if (animation < 500) {
                    seq = (battle_unit_seq_header_t*)unit->seq_data;
                    index = animation * 2 + animation_by_quadrant[quadrant];
                    if ((u32)index < seq->animation_count) {
                        if ((u32)index >= seq->first_animation) {
                            for (i = 0; i < 2; i++) {
                                if ((g_battle_gfx_vram_slots[i].owner & 0x1f) == unit->unit_id) {
                                    break;
                                }
                            }
                            if (i == 2) {
                                for (i = 0; i < 2; i++) {
                                    if (g_battle_gfx_vram_slots[i].owner == 0xff) {
                                        for (j = 0; j < 2; j++) {
                                            if (g_battle_gfx_vram_slots[j].evtchr_load_marker == 0) {
                                                battle_gfx_load_unit_into_evtchr_slot(unit, i, j);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (i == 2) {
                                    ok = 0;
                                }
                                if (j == 2) {
                                    ok = 0;
                                }
                                if (ok == 0) {
                                    if (g_battle_gfx_vram_slots[1].owner - 0xfe >= 2) {
                                        other = battle_unit_get_misc_data_by_misc_id(
                                            (u16)g_battle_gfx_vram_slots[1].owner & 0x1f);
                                        if (other != 0) {
                                            if (other->encoded_animation & 1) {
                                                other->animation_frame = g_battle_gfx_targeted_frame_back
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            } else {
                                                other->animation_frame = g_battle_gfx_targeted_frame_front
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            }
                                            other->effect_vector.vz = 0;
                                            other->effect_vector.vy = 0;
                                            other->effect_vector.vx = 0;
                                            battle_gfx_load_trap_and_unit_frame_parts(other,
                                                (battle_unit_anim_state_t*)&other->sprite_graphic_trigger,
                                                other->animation_frame, other->encoded_animation);
                                            ok = 1;
                                            unit->animation_countdown = 0;
                                            battle_gfx_load_unit_into_evtchr_slot(unit, 1, 1);
                                        }
                                    }
                                }
                            }
                        } else {
                            battle_gfx_release_unit_vram_slot((u8*)unit);
                        }
                    }
                }
                if (ok != 0) {
                    for (i = 0; i < 3; i++) {
                        unit->sprite_blocks[i].trigger = 0;
                    }
                    animation = unit->requested_animation - 1;
                    if (animation >= 500) {
                        unit->encoded_animation = unit->requested_animation - 1;
                    } else {
                        unit->encoded_animation = animation * 2 + animation_by_quadrant[quadrant];
                    }
                    *(u16*)&unit->sprite_display_flags
                        = (*(u16*)&unit->sprite_display_flags & 0xfff9) | flip_by_quadrant[quadrant];
                    unit->camera_facing_quadrant.s = quadrant;
                    unit->camera_facing_sixteenth.s = sixteenth;
                    unit->animation_script_pos = 0;
                    unit->animation_countdown = 0;
                    unit->secondary_animation_state = 0;
                    unit->effect_vector.vz = 0;
                    unit->effect_vector.vy = 0;
                    unit->effect_vector.vx = 0;
                    unit->screen_offset.vz = 0;
                    unit->screen_offset.vy = 0;
                    unit->screen_offset.vx = 0;
                    unit->animation_wait_bias = 0;
                    unit->animation_frame_bias = 0;
                    unit->animation_loop_count = 0;
                    unit->animation_flags = 0;
                    battle_gfx_run_unit_seq_script(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 1);
                    unit->movement.word &= ~0x20000000;
                    unit->requested_animation = 0;
                    if (unit->mount_state == 2) {
                        other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
                        if (other != 0) {
                            other->animation_script_pos = 0;
                            other->animation_countdown = 0;
                            battle_gfx_run_unit_seq_script(
                                other, (battle_unit_anim_state_t*)&other->sprite_graphic_trigger, 1);
                        }
                    }
                } else if (unit->animation_countdown != 0) {
                    for (i = 0; i < g_animation_speed; i++) {
                        if (unit->animation_countdown != 0) {
                            unit->animation_countdown--;
                            if (unit->animation_countdown == 0) {
                                battle_gfx_run_unit_seq_script(
                                    unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 1);
                            }
                        }
                    }
                }
            } else {
                unit->requested_animation = 0;
            }
        }
    } else if (!(unit->movement.word & 0x01000000)) {
        if (sixteenth != unit->camera_facing_sixteenth.s) {
            encoded = unit->encoded_animation;
            if (encoded < 6) {
                unit->encoded_animation = walk_by_direction[sixteenth];
                *(u16*)&unit->sprite_display_flags
                    = (*(u16*)&unit->sprite_display_flags & 0xfff9) | flip_by_direction[sixteenth];
            } else {
                unit->encoded_animation = (encoded & 0xfffe) + animation_by_quadrant[quadrant];
                *(u16*)&unit->sprite_display_flags
                    = (*(u16*)&unit->sprite_display_flags & 0xfff9) | flip_by_quadrant[quadrant];
            }
            unit->camera_facing_quadrant.s = quadrant;
            unit->camera_facing_sixteenth.s = sixteenth;
            battle_gfx_run_unit_seq_script(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 0);
        } else if (unit->animation_countdown != 0) {
            for (i = 0; i < g_animation_speed; i++) {
                if (unit->animation_countdown != 0) {
                    unit->animation_countdown--;
                    if (unit->animation_countdown == 0) {
                        battle_gfx_run_unit_seq_script(
                            unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 1);
                    }
                }
                battle_unit_dispatch_distortion_animation(unit);
            }
        }
    } else if (unit->animation_countdown != 0) {
        for (i = 0; i < g_animation_speed; i++) {
            if (unit->animation_countdown != 0) {
                unit->animation_countdown--;
                if (unit->animation_countdown == 0) {
                    battle_gfx_run_unit_seq_script(unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, 1);
                }
            }
            battle_unit_dispatch_distortion_animation(unit);
        }
    }
    for (i = 0; i < 3; i++) {
        if (unit->sprite_blocks[i].trigger != 0 && unit->sprite_blocks[i].wait != 0) {
            for (j = 0; j < g_animation_speed; j++) {
                if (unit->sprite_blocks[i].wait != 0) {
                    unit->sprite_blocks[i].wait--;
                    if (unit->sprite_blocks[i].wait == 0) {
                        battle_gfx_run_wep_eff_seq_script(unit, &unit->sprite_blocks[i], i, 1);
                    }
                }
            }
        }
    }
}

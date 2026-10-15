#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Update one unit's sprite animation for the frame.
 *
 * The unit's facing relative to the camera yaw gives the quadrant and the
 * sixteenth of a turn it is seen from. A request for
 * BATTLE_UNIT_ANIMATION_STAND shows the standing sequence of that sixteenth.
 * Any other request becomes sequence 2 * animation, + 1 seen from behind
 * (a request from 500 up is the sequence id itself). Animations below 11
 * start only on the unit's frame in four unless forced, and a request for
 * the sequence already playing is dropped. A sequence of the SEQ file's attack
 * set needs the unit's attack image in an EVTCHR slot first: a free slot and
 * a free image buffer or, failing that, slot 1 taken from the unit that
 * holds it, which is left on its rest frame. Without a request, a change of
 * sixteenth turns the standing pose or swaps the running sequence for its
 * other side; otherwise the sequence and the three weapon/effect blocks
 * count their waits down by g_main_system_animation_speed.
 *
 * unit is initialized in its declaration so its copy precedes the table
 * copies, and the unread unused_10 sizes the target's frame. */
void battle_gfx_update_and_animate_unit_wep_eff(battle_unit_t* unit_data) {
    battle_unit_t* unit = unit_data;
    s16 unused_10[4]; /* Unread; occupies sp+0x10 like the target. */
    /* The flip tables hold BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP for the views drawn mirrored. */
    s16 flip_by_quadrant[4] = { 0, 0, 2, 2 };
    s16 back_by_quadrant[4] = { 0, 1, 1, 0 };
    s16 flip_by_direction[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0 };
    s16 stand_by_direction[16] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 3, 3, 2, 2, 1 };
    battle_unit_t* other;
    battle_gfx_unit_seq_table_t* seq;
    s32 direction;
    s16 quadrant;
    s16 sixteenth;
    s32 animation;
    s32 index;
    s32 ok;
    u16 sequence_id;
    s32 i;
    s32 j;

    if (unit->draw_enabled == 0) {
        return;
    }
    if (unit->position_copies_active != 0) {
        battle_gfx_shift_and_fill_position_copies(unit);
    }
    direction = (g_camera_render_state.vy + unit->facing) & 0xfff;
    quadrant = direction / 1024;
    sixteenth = direction / 256;
    if (unit->requested_animation != 0) {
        if (unit->requested_animation - 1 == BATTLE_UNIT_ANIMATION_STAND) {
            for (i = 0; i < 3; i++) {
                unit->sprite_blocks[i].trigger = 0;
            }
            unit->animation.sequence_id = stand_by_direction[sixteenth];
            unit->sprite_display_flags
                = (unit->sprite_display_flags
                      & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                | flip_by_direction[sixteenth];
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            unit->animation.script_pos = 0;
            unit->animation.wait = 0;
            unit->animation.restart_count = 0;
            unit->seq_offset.vz = 0;
            unit->seq_offset.vy = 0;
            unit->seq_offset.vx = 0;
            unit->screen_offset.vz = 0;
            unit->screen_offset.vy = 0;
            unit->screen_offset.vx = 0;
            unit->animation.wait_bias = 0;
            unit->animation.frame_bias = 0;
            unit->animation.repeat_count = 0;
            unit->animation.flags = 0;
            battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
            unit->requested_animation = 0;
            battle_gfx_release_unit_vram_slot(unit);
        } else if (unit->requested_animation >= 12 || (unit->runtime_id & 3) == g_frame_counter % 4
            || (unit->movement.force_animation_update)) {
            /* An animation below 11 waits for the unit's frame in four. */
            ok = 1;
            if ((unit->requested_animation - 1) * 2 + back_by_quadrant[quadrant] != unit->animation.sequence_id
                || unit->animation.wait == 0 || (unit->movement.force_animation_update)) {
                animation = unit->requested_animation - 1;
                if (animation < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    seq = unit->animation.seq;
                    index = animation * 2 + back_by_quadrant[quadrant];
                    if (index < (u32)seq->first_sp2_sequence) {
                        if (index >= (u32)seq->first_attack_sequence) {
                            for (i = 0; i < 2; i++) {
                                if ((g_battle_gfx_vram_slots[i].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK)
                                    == unit->runtime_id) {
                                    break;
                                }
                            }
                            if (i == 2) {
                                for (i = 0; i < 2; i++) {
                                    if (g_battle_gfx_vram_slots[i].owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
                                        for (j = 0; j < 2; j++) {
                                            if (g_battle_gfx_vram_slots[j].image_buffer_state
                                                == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
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
                                    if (g_battle_gfx_vram_slots[1].owner - BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR >= 2) {
                                        other = battle_unit_get_by_runtime_id(
                                            g_battle_gfx_vram_slots[1].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK);
                                        if (other != 0) {
                                            if (other->animation.sequence_id & 1) {
                                                other->animation.frame = g_battle_gfx_targeted_frame_back
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            } else {
                                                other->animation.frame = g_battle_gfx_targeted_frame_front
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            }
                                            other->seq_offset.vz = 0;
                                            other->seq_offset.vy = 0;
                                            other->seq_offset.vx = 0;
                                            battle_gfx_load_trap_and_unit_frame_parts(other, &other->animation,
                                                other->animation.frame, other->animation.sequence_id);
                                            ok = 1;
                                            unit->animation.wait = 0;
                                            battle_gfx_load_unit_into_evtchr_slot(unit, 1, 1);
                                        }
                                    }
                                }
                            }
                        } else {
                            battle_gfx_release_unit_vram_slot(unit);
                        }
                    }
                }
                if (ok != 0) {
                    for (i = 0; i < 3; i++) {
                        unit->sprite_blocks[i].trigger = 0;
                    }
                    animation = unit->requested_animation - 1;
                    if (animation >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                        unit->animation.sequence_id = unit->requested_animation - 1;
                    } else {
                        unit->animation.sequence_id = animation * 2 + back_by_quadrant[quadrant];
                    }
                    unit->sprite_display_flags
                        = (unit->sprite_display_flags
                              & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                        | flip_by_quadrant[quadrant];
                    unit->camera_facing_quadrant = quadrant;
                    unit->camera_facing_sixteenth = sixteenth;
                    unit->animation.script_pos = 0;
                    unit->animation.wait = 0;
                    unit->animation.restart_count = 0;
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    unit->screen_offset.vz = 0;
                    unit->screen_offset.vy = 0;
                    unit->screen_offset.vx = 0;
                    unit->animation.wait_bias = 0;
                    unit->animation.frame_bias = 0;
                    unit->animation.repeat_count = 0;
                    unit->animation.flags = 0;
                    battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                    unit->movement.force_animation_update = 0;
                    unit->requested_animation = 0;
                    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                        if (other != 0) {
                            other->animation.script_pos = 0;
                            other->animation.wait = 0;
                            battle_gfx_run_unit_seq_script(other, &other->animation, 1);
                        }
                    }
                } else if (unit->animation.wait != 0) {
                    for (i = 0; i < g_main_system_animation_speed; i++) {
                        if (unit->animation.wait != 0) {
                            unit->animation.wait--;
                            if (unit->animation.wait == 0) {
                                battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                            }
                        }
                    }
                }
            } else {
                unit->requested_animation = 0;
            }
        }
    } else if (!(unit->movement.suppress_facing_update)) {
        if (sixteenth != unit->camera_facing_sixteenth) {
            sequence_id = unit->animation.sequence_id;
            if (sequence_id < (BATTLE_UNIT_ANIMATION_IDLE << 1)) {
                unit->animation.sequence_id = stand_by_direction[sixteenth];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags
                          & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_direction[sixteenth];
            } else {
                unit->animation.sequence_id = (sequence_id & 0xfffe) + back_by_quadrant[quadrant];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags
                          & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_quadrant[quadrant];
            }
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            battle_gfx_run_unit_seq_script(unit, &unit->animation, 0);
        } else if (unit->animation.wait != 0) {
            for (i = 0; i < g_main_system_animation_speed; i++) {
                if (unit->animation.wait != 0) {
                    unit->animation.wait--;
                    if (unit->animation.wait == 0) {
                        battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                    }
                }
                battle_unit_dispatch_distortion_animation(unit);
            }
        }
    } else if (unit->animation.wait != 0) {
        for (i = 0; i < g_main_system_animation_speed; i++) {
            if (unit->animation.wait != 0) {
                unit->animation.wait--;
                if (unit->animation.wait == 0) {
                    battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                }
            }
            battle_unit_dispatch_distortion_animation(unit);
        }
    }
    for (i = 0; i < 3; i++) {
        if (unit->sprite_blocks[i].trigger != 0 && unit->sprite_blocks[i].wait != 0) {
            for (j = 0; j < g_main_system_animation_speed; j++) {
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

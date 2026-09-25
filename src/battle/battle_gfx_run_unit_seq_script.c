#include "fft/battle.h"
#include "psx/types.h"

/* Run a unit's SEQ animation script until it shows the next frame or ends.
 *
 * Control opcodes (0xbe-0xff after a 0xff escape) move, loop, branch and start
 * sub-sprite animations; a plain byte pair is a frame and its wait. With
 * `advance` clear the current frame is reloaded without resetting its wait. */
void battle_gfx_run_unit_seq_script(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* state, s32 advance) {
    battle_gfx_shp0_t* seq;
    battle_unit_sprite_block_t* block;
    battle_unit_misc_data_t* target;
    u16 anim;
    u16 anim_index;
    u16 frame;
    u16 i;
    s16 pos;
    s16 attack_id;
    u16 count;
    s16 offset;
    u8 block_id;
    u8 block_anim;
    u8 timer;

    anim = state->animation_id;
    pos = state->script_pos;
    anim_index = anim;
    if (anim < 500) {
        seq = state->seq;
    } else if (anim < 600) {
        anim_index = anim - 500;
        seq = &g_battle_gfx_vram_slots[0].shp0;
    } else {
        seq = &g_battle_gfx_vram_slots[1].shp0;
        anim_index = anim - 600;
    }
    if (!advance) {
        pos -= 2;
    }
    if (seq->frames[anim_index] == (u8*)-1) {
        main_system_handle_animation_exception(9);
    }
    for (;;) {
        u8 op;

        if (seq->frames[anim_index][pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq->frames[anim_index][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                state->flags ^= 2;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                state->flags ^= 4;
                break;
            case BATTLE_SEQ_OP_SET_SHADOW:
                unit->shadow_graphic_trigger = 1;
                break;
            case BATTLE_SEQ_OP_CLEAR_SHADOW:
                unit->shadow_graphic_trigger = 0;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                if (anim < 500) {
                    if (anim % 2) {
                        frame = g_battle_gfx_targeted_frame_back[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    } else {
                        frame = g_battle_gfx_targeted_frame_front[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    }
                    unit->effect_vector.vz = 0;
                    unit->effect_vector.vy = 0;
                    unit->effect_vector.vx = 0;
                    battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, anim);
                }
                state->wait = 0;
                return;
            case BATTLE_SEQ_OP_INCREMENT_LOOP:
                pos = 0;
                state->counter_0c++;
                break;
            case BATTLE_SEQ_OP_SET_Y_ROTATION_0:
                state->display->y_rotation = 0;
                break;
            case BATTLE_SEQ_OP_POST_GENERIC_ATTACK:
                if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
                    break;
                }
                attack_id = unit->last_attack_id;
                if (attack_id == 0 || attack_id == 0x16f || (attack_id >= 0x8a && attack_id < 0x92)
                    || (attack_id >= 0xd5 && attack_id < 0xd8) || (attack_id >= 0x18a && attack_id < 0x19e)) {
                    if (unit->target_count != 0) {
                        for (i = 0; i < unit->target_count; i++) {
                            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
                            if (target != 0) {
                                battle_unit_set_target_animation_from_attack_type(unit, target);
                                battle_unit_set_attacker_animation_for_shield_block(unit, target);
                                battle_gfx_prepare_post_action_display_by_misc_id(target->unit_id);
                                battle_effect_set_evade_type_data_and_weapon_element(unit, 0);
                            }
                        }
                    }
                }
                break;
            case BATTLE_SEQ_OP_QUEUE_THROW_ANIMATION:
                battle_effect_set_evade_type_data_item_and_throw_stone_hardcoding(unit);
            case BATTLE_SEQ_OP_UNKNOWN_C7:
            case BATTLE_SEQ_OP_UNKNOWN_C8:
            case BATTLE_SEQ_OP_UNKNOWN_C9:
            case BATTLE_SEQ_OP_UNKNOWN_E6:
            case BATTLE_SEQ_OP_UNKNOWN_E7:
            case BATTLE_SEQ_OP_UNKNOWN_E8:
            case BATTLE_SEQ_OP_UNKNOWN_EA:
            case BATTLE_SEQ_OP_UNKNOWN_F1:
            case BATTLE_SEQ_OP_UNKNOWN_F8:
            case BATTLE_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case BATTLE_SEQ_OP_RELOAD_ANIMATION:
                anim = state->saved_animation_id;
                pos = state->saved_script_pos;
                anim_index = anim;
                if (anim >= 500) {
                    if (anim < 600) {
                        anim_index = anim - 500;
                    } else {
                        anim_index = anim - 600;
                    }
                }
                break;
            case BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_2:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq->frames[anim_index][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                battle_sound_play_movement_sfx(unit, seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
                battle_unit_shift_right_left(unit, seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
                battle_unit_add_signed_byte_to_height(unit, (s8)seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
                battle_unit_shift_forward_or_backward(unit, (s8)seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_SET_SLOWDOWN:
                state->wait_bias = (s8)seq->frames[anim_index][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_OFFSET:
                state->frame_bias = (s8)seq->frames[anim_index][pos++];
                break;
            case BATTLE_SEQ_OP_OVERRIDE_ANIMATION:
                state->saved_animation_id = anim;
                anim = seq->frames[anim_index][pos++];
                anim_index = anim;
                if (anim >= 500) {
                    if (anim < 600) {
                        anim_index = anim - 500;
                    } else {
                        anim_index = anim - 600;
                    }
                }
                state->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq->frames[anim_index][pos++];
                break;
            case BATTLE_SEQ_OP_QUEUE_SPRITE_ANIM:
                block_id = seq->frames[anim_index][pos++];
                block_anim = seq->frames[anim_index][pos++];
                if (block_id == 0) {
                    main_system_handle_animation_exception(0xd);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->wait = 1;
                block->animation_id = block_anim;
                block->script_pos = 0;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->loop_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_WAIT:
                offset = (s8)seq->frames[anim_index][pos++];
                count = seq->frames[anim_index][pos++];
                if (state->loop_count != 0) {
                    if (--state->loop_count != 0) {
                        pos = offset + (s16)(pos - 4);
                    }
                } else {
                    pos = offset + (s16)(pos - 4);
                    state->loop_count = count;
                }
                break;
            case BATTLE_SEQ_OP_UNKNOWN_F9:
                unit->screen_offset.vx += (s8)seq->frames[anim_index][pos++];
                unit->screen_offset.vy += (s8)seq->frames[anim_index][pos++];
                break;
            case BATTLE_SEQ_OP_SAVE_Y_SPIN:
                /* The one-trip loop is load-bearing: flow weights register
                 * references by loop depth, and the extra weight lets `count`
                 * take a0 ahead of `offset` (target a0/a1 in 0xfc and 0xe5). */
                do {
                    count = seq->frames[anim_index][pos++];
                    count |= seq->frames[anim_index][pos++] << 8;
                    state->display->y_rotation = count;
                } while (0);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT:
                battle_unit_shift_right_left(unit, seq->frames[anim_index][pos++]);
                battle_unit_add_signed_byte_to_height(unit, (s8)seq->frames[anim_index][pos++]);
                battle_unit_shift_forward_or_backward(unit, (s8)seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_MOVE_FORWARD_1:
                battle_unit_shift_forward_or_backward(unit, 1);
                break;
            case BATTLE_SEQ_OP_MOVE_FORWARD_2:
                battle_unit_shift_forward_or_backward(unit, 2);
                break;
            case BATTLE_SEQ_OP_MOVE_DOWN_1:
                battle_unit_add_signed_byte_to_height(unit, 1);
                break;
            case BATTLE_SEQ_OP_MOVE_DOWN_2:
                battle_unit_add_signed_byte_to_height(unit, 2);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_1:
                battle_unit_shift_forward_or_backward(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_2:
                battle_unit_shift_forward_or_backward(unit, -2);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_1:
                battle_unit_add_signed_byte_to_height(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_2:
                battle_unit_add_signed_byte_to_height(unit, -2);
                break;
            case BATTLE_SEQ_OP_LOAD_MF_ITEM:
                unit->item_ability_display = 1;
                break;
            case BATTLE_SEQ_OP_UNLOAD_MF_ITEM:
                unit->item_ability_display = 0;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_INPUT:
                if (g_battle_action_post_action == 0) {
                    offset = (s8)seq->frames[anim_index][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_ATTACK_SOUND:
                battle_sound_select_weapon_hit(unit, seq->frames[anim_index][pos++]);
                break;
            case BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_1:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq->frames[anim_index][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_MF_ITEM_POS:
                unit->item_get_x_offset = seq->frames[anim_index][pos++];
                unit->item_get_y_offset = seq->frames[anim_index][pos++];
                break;
            case BATTLE_SEQ_OP_QUEUE_DISTORT_ANIM:
                unit->distortion_animation_id = seq->frames[anim_index][pos++] + 2;
                timer = seq->frames[anim_index][pos++];
                unit->distortion_phase = 0;
                unit->distortion_timer = timer;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_DISTORT:
                if (unit->distortion_animation_id != 0) {
                    offset = (s8)seq->frames[anim_index][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_HOLD_WEAPON:
                offset = (s8)seq->frames[anim_index][pos];
                pos = offset + (s16)(pos - 2);
                break;
            case BATTLE_SEQ_OP_UNKNOWN_BF:
                battle_gfx_init_position_vector_copies(unit);
                break;
            case BATTLE_SEQ_OP_UNKNOWN_BE:
                unit->position_copies_active = 0;
                break;
            case BATTLE_SEQ_OP_UNKNOWN_C2:
                break;
            case BATTLE_SEQ_OP_UNKNOWN_CA:
            case BATTLE_SEQ_OP_UNKNOWN_D7:
            case BATTLE_SEQ_OP_UNKNOWN_E3:
            case BATTLE_SEQ_OP_UNKNOWN_E4:
            case BATTLE_SEQ_OP_UNKNOWN_E9:
            case BATTLE_SEQ_OP_UNKNOWN_ED:
            case BATTLE_SEQ_OP_UNKNOWN_F3:
            case BATTLE_SEQ_OP_UNKNOWN_F4:
            case BATTLE_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            default:
                main_system_handle_animation_exception(0xd);
                break;
            }
        } else {
            frame = state->frame_bias + seq->frames[anim_index][pos++];
            if (advance) {
                state->wait = seq->frames[anim_index][pos++] + state->wait_bias;
                if (state->wait > 0x100) {
                    state->wait = 1;
                }
            } else {
                pos++;
            }
            battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, anim);
            state->script_pos = pos;
            state->frame = frame;
            state->animation_id = anim;
            return;
        }
    }
}

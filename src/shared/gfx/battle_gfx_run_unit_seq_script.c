#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Run a unit's SEQ sequence up to its next frame instruction, or to its end.
 *
 * `state->sequence_id` selects the sequence: one of the unit's own SEQ table,
 * or sequence n of an EVTCHR slot for 500 + n and 600 + n. The commands in
 * front of the frame instruction run at once (battle_seq_opcode_e); then the
 * frame's parts are expanded into the unit's display and the state keeps the
 * frame, its wait and the position after it. With `advance` clear the
 * position first steps back to the frame instruction read last, which shows
 * that frame again without restarting its wait.
 *
 * A sequence that ends leaves a unit on its SHP type's
 * g_battle_gfx_targeted_frame_front or _back frame with a wait of 0. The
 * unpackers resolve a missing sequence to the start of the sequence data,
 * never to -1, so the MAIN_SYSTEM_ERROR_SEQ_ANIMATION test cannot fire. */
void battle_gfx_run_unit_seq_script(battle_unit_t* unit, battle_unit_anim_state_t* state, s32 advance) {
    battle_gfx_unit_seq_table_t* seq;
    battle_unit_sprite_block_t* block;
    battle_unit_t* target;
    u16 sequence_id;
    u16 index;
    u16 frame;
    u16 i;
    s16 pos;
    s16 attack_id;
    u16 value;
    s16 offset;
    u8 block_id;
    u8 block_sequence;
    u8 timer;

    sequence_id = state->sequence_id;
    pos = state->script_pos;
    index = sequence_id;
    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
        seq = state->seq;
    } else if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
        seq = &g_battle_gfx_vram_slots[0].seq.header;
    } else {
        seq = &g_battle_gfx_vram_slots[1].seq.header;
        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
    }
    if (!advance) {
        pos -= 2;
    }
    if (seq->sequences[index] == (u8*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        u8 op;

        if (seq->sequences[index][pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq->sequences[index][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                state->flags ^= BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                state->flags ^= BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case BATTLE_SEQ_OP_SHOW_SHADOW:
                unit->shadow_graphic_trigger = 1;
                break;
            case BATTLE_SEQ_OP_HIDE_SHADOW:
                unit->shadow_graphic_trigger = 0;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id % 2) {
                        frame = g_battle_gfx_targeted_frame_back[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    } else {
                        frame = g_battle_gfx_targeted_frame_front[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    }
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, sequence_id);
                }
                state->wait = 0;
                return;
            case BATTLE_SEQ_OP_RESTART:
                pos = 0;
                state->restart_count++;
                break;
            case BATTLE_SEQ_OP_CLEAR_ROTATION:
                state->display->rotation = 0;
                break;
            case BATTLE_SEQ_OP_STRIKE_TARGETS:
                if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
                    break;
                }
                /* The abilities whose hit is the weapon swing itself: a plain
                 * attack (0), the frog's, the Battle Skill breaks, the Snipe
                 * shots, Jump and Charge. */
                attack_id = unit->strike.last_attack_id;
                if (attack_id == 0 || attack_id == ABILITY_ID_FROG_ATTACK
                    || (attack_id >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK
                        && attack_id <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
                    || (attack_id >= ABILITY_ID_SNIPE_LEG_AIM && attack_id <= ABILITY_ID_SNIPE_SEAL_EVIL)
                    || (attack_id >= ABILITY_ID_JUMP_FIRST && attack_id < ABILITY_ID_MATH_FIRST)) {
                    if (unit->strike.target_count != 0) {
                        for (i = 0; i < unit->strike.target_count; i++) {
                            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                            if (target != 0) {
                                battle_unit_set_target_animation_from_attack_type(unit, target);
                                battle_unit_set_attacker_animation_for_shield_block(unit, target);
                                battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
                                effect_set_evade_type_data_and_weapon_element(unit, 0);
                            }
                        }
                    }
                }
                break;
            case BATTLE_SEQ_OP_LAUNCH_PROJECTILE:
                effect_start_action_projectile_secondary(unit);
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
            case BATTLE_SEQ_OP_RETURN:
                sequence_id = state->saved_sequence_id;
                pos = state->saved_script_pos;
                index = sequence_id;
                if (sequence_id >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                value = seq->sequences[index][pos++];
                sound_play_movement_sfx(unit, value);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
                battle_unit_shift_right_or_left(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
                battle_unit_shift_down_or_up(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
                battle_unit_shift_forward_or_backward(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_SET_WAIT_BIAS:
                state->wait_bias = (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_BIAS:
                state->frame_bias = (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_CALL_SEQUENCE:
                state->saved_sequence_id = sequence_id;
                sequence_id = seq->sequences[index][pos++];
                index = sequence_id;
                if (sequence_id >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                state->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq->sequences[index][pos++];
                block_sequence = seq->sequences[index][pos++];
                if (block_id == 0) {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->wait = 1;
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_REPEAT:
                offset = (s8)seq->sequences[index][pos++];
                value = seq->sequences[index][pos++];
                if (state->repeat_count != 0) {
                    if (--state->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    state->repeat_count = value;
                }
                break;
            case BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq->sequences[index][pos++];
                unit->screen_offset.vy += (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_SET_ROTATION:
                value = seq->sequences[index][pos++];
                value |= seq->sequences[index][pos++] << 8;
                state->display->rotation = value;

                break;
            case BATTLE_SEQ_OP_MOVE_UNIT:
                battle_unit_shift_right_or_left(unit, seq->sequences[index][pos++]);
                battle_unit_shift_down_or_up(unit, seq->sequences[index][pos++]);
                battle_unit_shift_forward_or_backward(unit, seq->sequences[index][pos++]);
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
                battle_unit_shift_down_or_up(unit, 1);
                break;
            case BATTLE_SEQ_OP_MOVE_DOWN_2:
                battle_unit_shift_down_or_up(unit, 2);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_1:
                battle_unit_shift_forward_or_backward(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_2:
                battle_unit_shift_forward_or_backward(unit, -2);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_1:
                battle_unit_shift_down_or_up(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_2:
                battle_unit_shift_down_or_up(unit, -2);
                break;
            case BATTLE_SEQ_OP_SHOW_ITEM:
                unit->item_display_active = 1;
                break;
            case BATTLE_SEQ_OP_HIDE_ITEM:
                unit->item_display_active = 0;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_INPUT:
                if (g_action_post_action == 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_WEAPON_SOUND:
                sound_play_weapon_sfx(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_SET_ITEM_POSITION:
                unit->item_get_x_offset = seq->sequences[index][pos++];
                unit->item_get_y_offset = seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_START_DISTORTION:
                unit->distortion_animation_id = seq->sequences[index][pos++] + 2;
                timer = seq->sequences[index][pos++];
                unit->distortion_phase = 0;
                unit->distortion_timer = timer;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_DISTORTION:
                if (unit->distortion_animation_id != BATTLE_DISTORTION_ANIMATION_NONE) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_JUMP:
                offset = (s8)seq->sequences[index][pos];
                pos += offset - 2;
                break;
            case BATTLE_SEQ_OP_START_POSITION_COPIES:
                battle_gfx_init_position_vector_copies(unit);
                break;
            case BATTLE_SEQ_OP_STOP_POSITION_COPIES:
                unit->position_copies_active = 0;
                break;
            case BATTLE_SEQ_OP_NOP:
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
                main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = state->frame_bias + seq->sequences[index][pos++];
            if (advance) {
                state->wait = seq->sequences[index][pos++] + state->wait_bias;
                /* A bias below the wait wraps the 16-bit value: wait one frame. */
                if (state->wait > 0x100) {
                    state->wait = 1;
                }
            } else {
                pos++;
            }
            battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, sequence_id);
            state->script_pos = pos;
            state->frame = frame;
            state->sequence_id = sequence_id;
            return;
        }
    }
}

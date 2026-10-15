#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Expand a frame into the display's parts, all drawn from texture page
 * `tpage`: that of the EVTCHR slot holding the frame's pictures. */
void battle_gfx_load_unit_frame_parts(
    battle_unit_t* unit, battle_gfx_sprite_display_data_t* display, battle_gfx_source_frame_t* src, s32 tpage) {
    battle_gfx_source_part_t* part;
    u32 i;
    u32 count;
    u8 flags;
    u16 attr;
    u16 size;
    u32 tile;
    s32 u;
    s32 v;
    s32 width;
    s32 height;

    flags = src->flags;
    count = display->part_count;
    display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | tpage;
    display->clut = unit->clut;
    for (i = 0; i < count; i++) {
        part = &src->parts[i];
        attr = part->attributes;
        size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
        width = g_battle_gfx_part_sizes[size].width;
        height = g_battle_gfx_part_sizes[size].height;
        tile = attr & BATTLE_SHP_PART_TILE_MASK;
        u = (tile & 0x1f) * 8;
        v = (tile >> 5) * 8;
        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8,
            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
    }
}

/* Return the frame tables that a unit frame id indexes and the frame itself:
 * a battle_gfx_source_frame_t pointer.
 *
 * A frame below BATTLE_SHP_FRAME_EVTCHR_FIRST is one of the unit's own SHP.
 * Its submerged table replaces the plain one for a rider that is not
 * jumping, and for a unit on a water tile whose surface is 12 or more above
 * the unit's feet; from 36 above, the unit shows submerged frame 0 whatever
 * the frame id. A unit that floats or moves on water, a crystal and a
 * treasure keep the plain table, and a chicken or a frog changes at 4.
 * Higher frame ids index the SHP of an EVTCHR slot: slot 0 for the sequences
 * below 600 and slot 1 for the rest, the submerged table on a water tile. */
void battle_gfx_select_unit_shp_frame(battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id,
    battle_gfx_unit_shp_frame_tables_t** out_table, battle_gfx_source_frame_t** out_frame) {
    map_tile_t* tile;
    battle_gfx_unit_shp_frame_tables_t* table;
    battle_gfx_source_frame_t* selected;
    s32 flags;
    s32 height;
    s32 z;

    tile = map_get_tile_data_pointer(
        (unit->world_coords.vx / MAP_TILE_SIZE), (unit->world_coords.vz / MAP_TILE_SIZE), unit->map_z);
    if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        table = state->shp;
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
                selected = table->frames[frame];
            } else {
                selected = table->submerged_frames[frame];
            }
        } else if (tile->depth_half_height.depth) {
            if (unit->stats != 0) {
                flags = move_get_effective_flags(unit->stats);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_FLOAT) {
                    selected = table->frames[frame];
                } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) || (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_TREASURE) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CHICKEN
                    || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_FROG) {
                    if ((tile->height + tile->depth_half_height.depth) * 12 >= -unit->world_coords.vy + 4) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                } else {
                    height = (tile->height + tile->depth_half_height.depth) * 12;
                    z = -unit->world_coords.vy;
                    if (height >= z + 0x24) {
                        selected = table->submerged_frames[0];
                    } else if (height >= z + 0xc) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                }
            } else {
                height = (tile->height + tile->depth_half_height.depth) * 12;
                z = -unit->world_coords.vy;
                if (height >= z + 0x24) {
                    selected = table->submerged_frames[0];
                } else if (height >= z + 0xc) {
                    selected = table->submerged_frames[frame];
                } else {
                    selected = table->frames[frame];
                }
            }
        } else {
            selected = table->frames[frame];
        }
    } else {
        table = &g_battle_gfx_vram_slots[1].shp.tables;
        if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
            table = &g_battle_gfx_vram_slots[0].shp.tables;
        }
        if (tile->depth_half_height.depth) {
            selected = table->submerged_frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        } else {
            selected = table->frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        }
    }
    *out_table = table;
    *out_frame = selected;
}

/* Expand one SHP frame into the parts of a unit's sprite display.
 *
 * Where the frame's pictures are decides the texture page: the OTHER page
 * for the crystal, chicken, frog and treasure sheets; the unit's spritesheet
 * slot for frames below the SHP's first attack frame; above that, the EVTCHR
 * slot that holds the unit's attack image (no slot, no parts). Frames from
 * BATTLE_SHP_FRAME_EVTCHR_FIRST up belong to an EVTCHR slot's own SHP and
 * the sequence id says which slot.
 *
 * A 48x48 part from a spritesheet slot ends the frame: it is stored as two
 * 48x24 halves, the upper one flagged to draw nearer the camera. The two
 * Altima sheets keep theirs whole. */
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id) {
    battle_gfx_unit_shp_frame_tables_t* frame_table;
    battle_gfx_source_frame_t* frame_data;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_source_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    s16 part_flags;
    u32 row;

    y_offset = unit->special_graphic_y_offset;
    /* The unpackers never store the -1 tested here, and three bits
     * of part count cannot pass 8. */
    battle_gfx_select_unit_shp_frame(unit, state, frame, sequence_id, &frame_table, &frame_data);
    if (frame_data == (battle_gfx_source_frame_t*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
    }
    display = state->display;
    count = display->part_count = (frame_data->part_count_and_rotation & 7) + 1;
    display->rotation = g_battle_gfx_sprite_y_rotations[frame_data->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        main_system_handle_error(MAIN_SYSTEM_ERROR_FRAME_PART_COUNT);
    }
    flags = frame_data->flags;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_OTHER;
        display->clut = unit->clut;
        for (i = 0; i < count; i++) {
            part = &frame_data->parts[i];
            attr = part->attributes;
            size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = g_battle_gfx_part_sizes[size].width;
                height = g_battle_gfx_part_sizes[size].height;
                tile = attr & BATTLE_SHP_PART_TILE_MASK;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                    width * 8, height * 8,
                    (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
            }
        }
    } else if (frame < (u32)state->shp->first_attack_frame) {
        display->tpage = unit->tpage | (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        display->clut = unit->clut;
        i = 0;
        if (count != 0) {
            do {
                part = &frame_data->parts[i];
                attr = part->attributes;
                size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                if (size == BATTLE_SHP_PART_SIZE_48X48) {
                    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & BATTLE_SHP_PART_TILE_MASK;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = g_battle_gfx_part_sizes[size].width;
                        height = g_battle_gfx_part_sizes[size].height;
                        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                            width * 8, height * 8,
                            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                    } else {
                        tile_index = attr & BATTLE_SHP_PART_TILE_MASK;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags
                            = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                        battle_gfx_store_sprite_display_data(
                            display, i, part->x_shift, part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18, part_flags);
                        battle_gfx_store_sprite_display_data(display, i + 1, part->x_shift, part->y_shift, u, row * 8,
                            0x30, 0x18, part_flags | BATTLE_SPRITE_DISPLAY_NEARER);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                        width * 8, height * 8,
                        (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        if ((g_battle_gfx_vram_slots[0].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
        } else if ((g_battle_gfx_vram_slots[1].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
        }
    } else if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
    } else {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
    }
}

/* SEQ script step: moves the sprite offset by delta across the unit's facing: +x
 * facing north, -x facing south, +z facing west, -z facing east. A diagonal
 * facing matches no case. */
void battle_unit_shift_right_or_left(battle_unit_t* unit, s8 delta) {
    s16* vector;
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_SOUTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector - delta;
        return;
    case BATTLE_FACING_WEST:
        vector = &unit->seq_offset.vz;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_EAST:
        vector = &unit->seq_offset.vz;
        *vector = *vector - delta;
        return;
    }
}

/* SEQ script step: moves the sprite offset by delta along the height axis;
 * positive is down. */
void battle_unit_shift_down_or_up(battle_unit_t* unit, s8 delta) {
    unit->seq_offset.vy += delta;
}

/* SEQ script step: moves the sprite offset by delta along the unit's facing
 * (north is +z, east is +x). A diagonal facing matches no case. */
void battle_unit_shift_forward_or_backward(battle_unit_t* unit, s8 delta) {
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        unit->seq_offset.vz += delta;
        break;
    case BATTLE_FACING_SOUTH:
        unit->seq_offset.vz -= delta;
        break;
    case BATTLE_FACING_WEST:
        unit->seq_offset.vx -= delta;
        break;
    case BATTLE_FACING_EAST:
        unit->seq_offset.vx += delta;
        break;
    }
}

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

/* Rebuilds the casting unit's sprite parts for its current animation and frame. */
void battle_gfx_load_casting_unit_graphics(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    if (unit != 0) {
        /* The sprite data block starts at the graphic trigger; 0x1e0 is the
         * current frame. */
        battle_gfx_load_trap_and_unit_frame_parts(
            unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Run a weapon or effect sprite block's sequence up to its next frame
 * instruction, or to its end, as battle_gfx_run_unit_seq_script does for the
 * unit's own sprite. The unit-only commands are errors here, the ones that
 * move a unit only skip their operands, and BATTLE_SEQ_OP_STRIKE_TARGETS and
 * the two shadow commands do nothing.
 *
 * A frame id counts from the first frame of the unit's weapon type
 * (battle_gfx_weapon_shp_t.first_frame). The frame's parts, three at most,
 * are expanded into the block's display: a weapon block draws them from the
 * weapon sheet, at the row of the used item's graphic, and an effect block
 * from the effect sheet. `index` is the block's number; it selects the CLUT
 * row.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
void battle_gfx_run_wep_eff_seq_script(
    battle_unit_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance) {
    battle_gfx_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 sequence_id;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    battle_gfx_source_frame_t* src;
    battle_gfx_sprite_display_data_t* display;
    battle_unit_sprite_block_t* block;
    battle_gfx_source_part_t* part;
    s16 offset;
    u16 value;
    u8 block_id;
    u8 block_sequence;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    sequence_id = sprite->sequence_id;
    graphic_v = g_battle_gfx_item_graphic_data[unit->used_item_or_weapon_id].texture_row * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (sequence_id > 0x100) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    if (seq[sequence_id] == (u8*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        data = seq[sequence_id];
        if (data[pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq[sequence_id][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
                sprite->trigger = 0;
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case BATTLE_SEQ_OP_RESTART:
                sprite->restart_count++;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_CLEAR_ROTATION:
                sprite->display->rotation = 0;
                break;
            case BATTLE_SEQ_OP_LAUNCH_PROJECTILE:
                effect_start_action_projectile_secondary(unit);
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
                pos = sprite->saved_script_pos;
                sequence_id = sprite->saved_sequence_id;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                value = seq[sequence_id][pos++];
                sound_play_movement_sfx(unit, value);
                break;
            case BATTLE_SEQ_OP_SET_WAIT_BIAS:
                sprite->wait_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_BIAS:
                sprite->frame_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_CALL_SEQUENCE:
                sprite->saved_sequence_id = sequence_id;
                sequence_id = seq[sequence_id][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq[sequence_id][pos++];
                unit->screen_offset.vy += (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_REPEAT:
                offset = (s8)seq[sequence_id][pos++];
                value = seq[sequence_id][pos++];
                if (sprite->repeat_count != 0) {
                    if (--sprite->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    sprite->repeat_count = value;
                }
                break;
            case BATTLE_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq[sequence_id][pos++];
                block_sequence = seq[sequence_id][pos++];
                if (block_id == 0) {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_SET_ROTATION:
                value = seq[sequence_id][pos++];
                value |= seq[sequence_id][pos++] << 8;
                sprite->display->rotation = value;

                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
            case BATTLE_SEQ_OP_MOVE_UNIT:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_JUMP:
                offset = (s8)seq[sequence_id][pos];
                pos += offset - 2;
                break;
            case BATTLE_SEQ_OP_UNKNOWN_D7:
            case BATTLE_SEQ_OP_UNKNOWN_E3:
            case BATTLE_SEQ_OP_UNKNOWN_E4:
            case BATTLE_SEQ_OP_UNKNOWN_E9:
            case BATTLE_SEQ_OP_UNKNOWN_ED:
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
            case BATTLE_SEQ_OP_UNKNOWN_F3:
            case BATTLE_SEQ_OP_UNKNOWN_F4:
            case BATTLE_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            case BATTLE_SEQ_OP_STRIKE_TARGETS:
            case BATTLE_SEQ_OP_HIDE_SHADOW:
            case BATTLE_SEQ_OP_SHOW_SHADOW:
                break;
            default:
                main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = sprite->frame_bias + (data[pos++] + shp->first_frame[unit->equipped_weapon_type]);
            if (advance != 0) {
                sprite->wait = data[pos++] + sprite->wait_bias;
            } else {
                pos++;
            }
            src = shp->frames[frame];
            if (frame > 0x200) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            if (src == (battle_gfx_source_frame_t*)-1) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->rotation = g_battle_gfx_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
                display->rotation = -display->rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case BATTLE_SPRITE_GRAPHIC_WEAPON:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_WEAPON;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            case BATTLE_SPRITE_GRAPHIC_EFFECT:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_EFFECT;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->sequence_id = sequence_id;
            return;
        }
    }
}

/* Capture a unit's graphics into an EVTCHR VRAM cache slot.
 *
 * The unit's compressed spritesheet slot is expanded into `image_slot`'s
 * image buffer and uploaded to `slot`'s rectangle; the unit's 64x32 VRAM
 * block (column slot >> 3, row slot & 7 of the 0x340,0x100 spritesheet area)
 * is copied 200 lines below it. `slot` is then owned by the unit (id | 0x80). */
void battle_gfx_load_unit_into_evtchr_slot(battle_unit_t* unit, s32 slot, s32 image_slot) {
    RECT rect;

    battle_gfx_decompress_attack_spritesheet(
        g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image,
        g_battle_gfx_vram_slots[image_slot].image_data);
    LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)g_battle_gfx_vram_slots[image_slot].image_data);
    rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
    rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
    rect.w = 0x40;
    rect.h = 0x20;
    MoveImage(&rect, g_battle_gfx_vram_slots[slot].image_rect.x, g_battle_gfx_vram_slots[slot].image_rect.y + 200);
    g_battle_gfx_vram_slots[slot].owner = unit->runtime_id | BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE;
    g_battle_frame_pacing_suppressed = 1;
    g_battle_gfx_vram_slots[image_slot].image_buffer_state = BATTLE_GFX_VRAM_SLOT_UPLOAD_FRAMES;
    g_battle_gfx_vram_slots[slot].evtchr_load_state = 1;
}

/* Free the EVTCHR slots whose page holds the unit's attack image. One that
 * holds its SP2 image stays. */
void battle_gfx_release_unit_vram_slot(battle_unit_t* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < 2; i++) {
        slot_id = g_battle_gfx_vram_slots[i].owner;
        if (slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR && slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_NONE
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) == 0) {
            g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
            g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
        }
    }
}

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

/* Draws the unit's status bubble when the unit is on screen and has one.
 *
 * Nothing is drawn unless the camera-relative position lies within x 0x61..0x19f
 * and y -0x1f..0x10f, the ordering-table depth within 1..0x17f,
 * sprite_graphic_trigger is set, the unit is not a mount carrying its rider and
 * status_bubble_active is set. */
void battle_gfx_update_status_bubble_display(battle_unit_t* unit) {
    DVECTOR screen_coords;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_coords.vx = unit->camera_relative_x;
    screen_coords.vy = unit->camera_relative_y;
    if (screen_coords.vx < 0x61 || screen_coords.vx > 0x19f) {
        return;
    }
    if (screen_coords.vy < -0x1f || screen_coords.vy > 0x10f) {
        return;
    }
    if ((unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    battle_gfx_draw_status_bubble(unit, &screen_coords);
}

/* Queue a unit's sprite layers into the ordering table at its depth.
 *
 * g_battle_gfx_animation_layer_priorities[layer_priority] lists four layers in draw order: 0 is the unit's own
 * sprite display, n > 0 is weapon/effect sprite block n - 1 (skipped while
 * its trigger is clear). A rider (mount state 1) is skipped here; a mount
 * (state 2) draws its first three body parts, then its rider's layers, then
 * the rest of its body. Unmounted units mirror their body sprite by the
 * animation state's flags. The shadow and the position copies follow.
 *
 * The rider loop issues one call per arm; cross-jumping merges them after
 * reload, but the duplicated RTL makes the mount loop large enough that
 * loop.c leaves `row * 16 + g_battle_gfx_animation_layer_priorities` inside it, which is why the target
 * recomputes the table address each iteration and spills `row`. */
void battle_gfx_draw_unit_sprite_layers(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    battle_unit_t* rider;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_sprite_display_data_t* rider_display;
    s32 row;
    s32 rider_row;
    s32 i;
    s32 j;
    s32 entry;
    s16 angle;
    s16 flags;
    s16 rider_flags;

    position.vx = unit->camera_relative_x;
    position.vy = unit->camera_relative_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display.active) {
        battle_gfx_animate_post_action_text(unit, &position);
    }
    if (!((position.vx >= 0x61 && position.vx <= 0x19f) && (position.vy >= -0x1f && position.vy <= 0x10f))
        && unit->spritesheet_id != BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        return;
    }
    copyVector(&zoom, &g_camera_zoom);
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags;
    row = unit->layer_priority;
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->item_display_active) {
        battle_gfx_init_item_get_rendering(unit);
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        } else if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = g_battle_gfx_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                } else {
                    display = unit->animation.display;
                    battle_gfx_construct_polygon_data_for_units(
                        display, 3, 0, &position, angle, flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                    rider = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    if (rider != 0 && rider->draw_enabled != 0) {
                        rider_position.vx = rider->camera_relative_x;
                        rider_position.vy = rider->camera_relative_y;
                        rider_flags = rider->sprite_display_flags;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = g_battle_gfx_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->animation.display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            }
                        }
                    }
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 3, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                }
            }
            if (unit->shadow_graphic_trigger) {
                battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            entry = g_battle_gfx_animation_layer_priorities[row][i];
            if (entry != 0) {
                if (unit->sprite_blocks[entry - 1].trigger == 0) {
                    continue;
                }
                display = unit->sprite_blocks[entry - 1].display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle, flags,
                    &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            } else {
                display = unit->animation.display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                    flags ^ unit->animation.flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
        if (unit->shadow_graphic_trigger) {
            battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
        }
    }
    if (unit->position_copies_active) {
        battle_gfx_draw_unit_sprite_position_copies(unit);
    }
}

/* Projects every display unit into camera space.
 *
 * Refreshes each item-effect origin and ordering-table depth. Terrain depth
 * lowers grounded units, while In Water, Float, and Walk on Water select
 * their original exceptions. */
void battle_gfx_update_all_unit_rotation_and_vectors(void) {
    VECTOR transformed;
    SVECTOR screen;
    SVECTOR adjusted;
    long gte_flag;
    battle_unit_t* unit;
    map_tile_t* tile;
    move_effective_flags_e movement_flags;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_copy_draw_coords(&screen, unit);
        RotTrans(&screen, &transformed, &unit->gte_flag);

        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
            unit->camera_relative_x = transformed.vx - unit->screen_offset.vx;
        } else {
            unit->camera_relative_x = transformed.vx + unit->screen_offset.vx;
        }
        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) != 0) {
            unit->camera_relative_y = transformed.vy - unit->screen_offset.vy;
        } else {
            unit->camera_relative_y = transformed.vy + unit->screen_offset.vy;
        }

        tile = map_get_tile_data_pointer(
            unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if ((tile->surface.depth_bias) != 0) {
                camera_calculate_relative_offset_by_5(&screen, &adjusted);
            } else {
                camera_calculate_relative_offset_by_1(&screen, &adjusted);
            }
        } else if ((tile->surface.depth_bias) != 0) {
            camera_convert_screen_coords_modify_by_5(&screen, &adjusted);
        } else {
            camera_convert_screen_coords_modify_by_1(&screen, &adjusted);
        }

        if (unit->stats != 0) {
            movement_flags = move_get_effective_flags(unit->stats);
            if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) != 0) {
                if (tile->depth_half_height.depth != 0) {
                    adjusted.vy -= 12;
                }
            } else if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) == 0) {
                if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) == 0) {
                    adjusted.vy -= tile->depth_half_height.depth * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

/*
 * Per-frame status-bubble and unit-graphics update.
 *
 * In the listed game states, bubbles are held off for 0x3c frames whenever the
 * camera zooms, tilts, rotates or moves, and processed for every runtime unit
 * Data record once the countdown has run out. Every state then reloads the
 * casting unit's graphics and runs battle_gfx_draw_unit_sprite_layers on every record.
 */
void battle_gfx_update_status_bubbles_and_graphics(void) {
    battle_unit_t* unit;

    switch (g_battle_game_state) {
    case BATTLE_GAME_STATE_FREE_CURSOR:
    case BATTLE_GAME_STATE_HIGHLIGHT_UNITS:
    case BATTLE_GAME_STATE_OPEN_ACTION_MENUS:
    case BATTLE_GAME_STATE_IDLING_ACTION_MENUS:
    case BATTLE_GAME_STATE_MENU_TO_TARGETING:
    case BATTLE_GAME_STATE_DISPLAY_MOVE_AREA:
    case BATTLE_GAME_STATE_UNIT_MOVE:
    case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
    case BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP:
    case BATTLE_GAME_STATE_TARGETING_RANGE:
    case BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING:
    case BATTLE_GAME_STATE_CONFIRM_ACTION:
        if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
            && g_camera_rotation_action == CAMERA_ROTATION_NONE
            && !(g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz)) {
            if (g_battle_gfx_status_bubble_delay != 0) {
                g_battle_gfx_status_bubble_delay -= g_main_system_animation_speed;
                if (g_battle_gfx_status_bubble_delay <= 0) {
                    g_battle_gfx_status_bubble_delay = 0;
                }
            } else {
                for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
                    battle_gfx_update_status_bubble_display(unit);
                }
            }
        } else {
            g_battle_gfx_status_bubble_delay = 0x3c;
        }
        break;
    }
    battle_gfx_load_casting_unit_graphics();
    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        battle_gfx_draw_unit_sprite_layers(unit);
    }
}

/* Inertia threshold backup, slot motion, particle integration, list nodes and frame scripts. */

#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

void effect_save_inertia_threshold(void) {
    g_effect_saved_inertia_threshold = g_effect_inertia_threshold;
}

void effect_restore_inertia_threshold(void) {
    g_effect_inertia_threshold = g_effect_saved_inertia_threshold;
}

/* Advance an effect slot's motion state by one frame. */
void effect_step_motion(effect_motion_t* motion) {
    s32 unused[4]; /* The target reserves an unused 16-byte frame. */

    motion->position.x += motion->velocity.x;
    motion->position.y += motion->velocity.y;
    motion->position.z += motion->velocity.z;
    motion->velocity.x
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.x + (motion->acceleration.x * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.x) >> 12);
    motion->velocity.y
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.y + (motion->acceleration.y * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.y) >> 12);
    motion->velocity.z
        = ((motion->inertia - g_effect_inertia_threshold) * motion->velocity.z + (motion->acceleration.z * ONE))
            / motion->inertia
        + ((motion->weight * g_effect_gravity.z) >> 12);
    motion->acceleration.x += motion->acceleration_step.x;
    motion->acceleration.y += motion->acceleration_step.y;
    motion->acceleration.z += motion->acceleration_step.z;
}

/* Integrate one frame of motion for every particle in a list.
 *
 * Applies motion integration to each node. A node with no
 * homing strength then adds its acceleration step to its acceleration. One
 * with a strength adds delta + (step - delta) * s / 127 instead, where delta
 * is the unit vector to its homing target scaled by the strength and s is
 * its homing curve's factor less 0x80: a factor of 0xff adds the step alone,
 * 0x80 the target vector alone, and 0, or no curve, twice the target vector
 * less the step. The list parameter doubles as the next-node cursor: that
 * keeps both loop tests on it. */
void effect_integrate_particle_motion(effect_work_record_t* list) {
    effect_work_record_t* node;
    effect_motion_t* motion;
    s32 weight;
    s32 strength;
    s32 track;
    VECTOR delta;
    s32 threshold;
    s32 gravity_x;
    s32 gravity_y;
    s32 gravity_z;

    threshold = g_effect_inertia_threshold;
    gravity_x = g_effect_gravity.x;
    gravity_y = g_effect_gravity.y;
    gravity_z = g_effect_gravity.z;

    node = list;
    while (list != 0) {
        list = node->next;
        motion = &node->motion;

        motion->position.x += motion->velocity.x;
        motion->position.y += motion->velocity.y;
        motion->position.z += motion->velocity.z;

        weight = motion->inertia - threshold;

        motion->velocity.x = (weight * motion->velocity.x + (motion->acceleration.x * ONE)) / motion->inertia
            + ((gravity_x * motion->weight) >> 12);
        motion->velocity.y = (weight * motion->velocity.y + (motion->acceleration.y * ONE)) / motion->inertia
            + ((gravity_y * motion->weight) >> 12);
        motion->velocity.z = (weight * motion->velocity.z + (motion->acceleration.z * ONE)) / motion->inertia
            + ((gravity_z * motion->weight) >> 12);

        weight = node->homing_strength;
        if (weight != 0) {
            track = node->homing_curve_selector;
            track--;
            if (track != -1) {
                strength = g_effect_animation_curve_data->curves[track][node->curve_frame] - 0x80;
            } else {
                strength = -0x80;
            }

            setVector(&delta, node->target.x - (motion->position.x >> 12), node->target.y - (motion->position.y >> 12),
                node->target.z - (motion->position.z >> 12));
            if (delta.vx != 0 || delta.vy != 0 || delta.vz != 0) {
                VectorNormal(&delta, &delta);
                setVector(&delta, (weight * delta.vx) >> 12, (weight * delta.vy) >> 12, (weight * delta.vz) >> 12);
            }

            motion->acceleration.x += ((motion->acceleration_step.x - delta.vx) * strength) / 127 + delta.vx;
            motion->acceleration.y += ((motion->acceleration_step.y - delta.vy) * strength) / 127 + delta.vy;
            motion->acceleration.z += ((motion->acceleration_step.z - delta.vz) * strength) / 127 + delta.vz;
        } else {
            motion->acceleration.x += motion->acceleration_step.x;
            motion->acceleration.y += motion->acceleration_step.y;
            motion->acceleration.z += motion->acceleration_step.z;
        }
        node = list;
    }
}

/*
 * Rebuilds the effect-node free list: node 0 is the list sentinel, nodes
 * 1..256 are chained through next with 256 terminating, and g_effect_free_list_node_head holds
 * the free-list head.  The GetClut result is computed and discarded in the
 * target.
 */
void effect_init_free_list(void) {
    s32 i;

    GetClut(0, 0x1f0);

    g_effect_list_nodes[0].next = 0;
    g_effect_list_nodes[0].prev = 0;
    g_effect_free_list_node_head = 1;
    for (i = 1; i < EFFECT_SPRITE_NODE_CAPACITY; i++) {
        g_effect_list_nodes[i].index = i;
        g_effect_list_nodes[i].prev = i - 1;
        g_effect_list_nodes[i].next = i + 1;
    }
    g_effect_list_nodes[i].prev = i - 1;
    g_effect_list_nodes[i].next = 0;
    g_effect_list_nodes[i].index = i;
    g_effect_active_list_node_head = 0;
}

/* Take a sprite node from the free list, push it on the head of the active list
 * and give it a one-sprite block; kind is an EFFECT_LIST_NODE_* value and the
 * animation fields start at 0.
 *
 * The free list is not tested for exhaustion: with none free the sentinel node 0
 * would be handed out. There are as many nodes as work records, and
 * effect_alloc_work_record is the only caller. */
effect_list_node_t* effect_alloc_list_node(s16 kind) {
    u16* free_head = &g_effect_free_list_node_head;
    u16* list_head = &g_effect_active_list_node_head;
    u16 index = *free_head;
    effect_list_node_t* node = &g_effect_list_nodes[index];

    *free_head = node->next;
    g_effect_list_nodes[node->next].prev = 0;
    g_effect_list_nodes[*list_head].prev = index;
    node->next = *list_head;
    node->prev = 0;
    *list_head = index;
    node->sprite_block = effect_alloc_sprite_block(1);
    node->screen_rotation_angle = 0;
    node->frame_timer = 0;
    node->sequence_data = 0;
    node->sequence_offset = 0;
    node->frame_group_offset = 0;
    node->sprite_offset_y = 0;
    node->sprite_offset_x = 0;
    node->kind = kind;
    node->sprite_frame_index = 0;
    return node;
}

void effect_free_list_node(effect_list_node_t* node) {
    u16* free_head;

    if (node->sprite_block != 0) {
        effect_free_sprite_block(node->sprite_block);
        node->sprite_block = 0;
    }
    if (node->prev != 0) {
        g_effect_list_nodes[node->prev].next = node->next;
    } else {
        g_effect_active_list_node_head = node->next;
    }
    g_effect_list_nodes[node->next].prev = node->prev;
    free_head = &g_effect_free_list_node_head;
    node->next = *free_head;
    node->prev = 0;
    *free_head = node->index;
}

/* Point a sprite animation node at parameter set animation_index and restart
 * it: clear the sequence position and hold time, store frame_group doubled and
 * set kind bit 0 to mark the frame dirty. */
void effect_init_animation_sequence(effect_list_node_t* state, s32 frame_group, s32 animation_index) {
    u8* sequences;
    u8* entry;
    s16 offset;
    u16 flags;

    sequences = g_effect_animation_sequences;
    /* Integer addresses preserve the offset/base operand order at both additions. */
    entry = (u8*)((s16)animation_index * 2 + (s32)sequences);
    flags = state->kind | EFFECT_LIST_NODE_FRAME_CHANGED;
    offset = entry[0] + (entry[1] << 8);
    state->sequence_offset = 0;
    state->frame_group_offset = (frame_group & 0xFF) * 2;
    state->frame_timer = 0;
    state->kind = flags;
    state->sequence_data = (u8*)(offset + (s32)sequences);
}

/* Advances a list node's frame script, rebuilds its sprite-part table when the
 * frame changes, and queues the sprite at the node position.
 *
 * Script bytes: a value below 0x80 is a frame id followed by a hold count and a
 * depth mode; 0x81 restarts the script, 0x82 sets the 16-bit x/y offset and
 * 0x83 adds a signed 8-bit x/y delta. kind bit 0 marks the frame dirty; bits
 * 1-2 select camera projection or a screen-space position, with or without
 * the camera zoom. The oversized frame is reproduced with unused locals. */
void effect_render_particle_sprite(effect_list_node_t* particle, s32 red, s32 green, s32 blue) {
    effect_list_node_t* node;
    SVECTOR input;
    SVECTOR unused_20;
    VECTOR output;
    s32 unused_38[6];
    DVECTOR position;
    effect_sprite_block_t* block;
    effect_sprite_frameset_t* frame;
    s16 frame_table_offset;
    s32 op;
    s16 count;
    s16 i;
    s32 depth;

    node = particle;
    block = node->sprite_block;
    node->frame_timer -= 2;
    if (node->frame_timer <= 0) {
        for (;;) {
            op = node->sequence_data[node->sequence_offset];
            if (!(op & EFFECT_SEQUENCE_OP_FLAG)) {
                if (node->sprite_frame_index != op) {
                    node->sprite_frame_index = op;
                    node->kind |= EFFECT_LIST_NODE_FRAME_CHANGED;
                }
                node->frame_timer = effect_get_current_sequence_instruction(node)->frame.hold;
                node->depth_mode = effect_get_current_sequence_instruction(node)->frame.depth_mode;
                if (node->frame_timer != 0) {
                    node->sequence_offset += 3;
                }
                break;
            }
            switch (op & EFFECT_SEQUENCE_OP_MASK) {
            case EFFECT_SEQUENCE_OP_LOOP:
                node->sequence_offset = 0;
                break;
            case EFFECT_SEQUENCE_OP_SET_OFFSET:
                node->sprite_offset_x = effect_get_current_sequence_instruction(node)->set_offset.x_le[0]
                    + (effect_get_current_sequence_instruction(node)->set_offset.x_le[1] << 8);
                node->sprite_offset_y = effect_get_current_sequence_instruction(node)->set_offset.y_le[0]
                    + (effect_get_current_sequence_instruction(node)->set_offset.y_le[1] << 8);
                node->sequence_offset += 5;
                break;
            case EFFECT_SEQUENCE_OP_ADD_OFFSET:
                node->sprite_offset_x += effect_get_current_sequence_instruction(node)->add_offset.x;
                node->sprite_offset_y += effect_get_current_sequence_instruction(node)->add_offset.y;
                node->sequence_offset += 3;
                break;
            }
        }
    }
    if (node->kind & EFFECT_LIST_NODE_FRAME_CHANGED) {
        frame_table_offset
            = ((effect_sprite_group_t*)(g_effect_frame_data + node->frame_group_offset))->frame_table_offset;
        frame = (effect_sprite_frameset_t*)(*(u16*)(g_effect_frame_data
                                                + (node->sprite_frame_index * 2 + frame_table_offset))
            + g_effect_frame_data);
        count = frame->count;
        if (block->header.sprite_count != count) {
            effect_free_sprite_block(block);
            block = effect_alloc_sprite_block(count);
            node->sprite_block = block;
        }
        for (i = 0; i < count; i++) {
            block->frames[i] = &frame->parts[i];
        }
        node->kind &= ~EFFECT_LIST_NODE_FRAME_CHANGED;
    }
    block->header.color.r = red;
    block->header.color.g = green;
    block->header.color.b = blue;
    switch (node->kind & EFFECT_LIST_NODE_PLACEMENT_MASK) {
    case EFFECT_LIST_NODE_WORLD:
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&input, node->x + node->sprite_offset_x, node->y + node->sprite_offset_y, node->z);
        RotTrans(&input, &output, &output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case EFFECT_SPRITE_DEPTH_NEARER_8:
            depth -= 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_8:
            depth = 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_FARTHEST:
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_16:
            depth = 16;
            break;
        case EFFECT_SPRITE_DEPTH_NEARER_16:
            depth -= 16;
            break;
        }
        if (depth < 0) {
            return;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        position.vx = output.vx;
        position.vy = output.vy;
        effect_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, &g_camera_zoom, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_SCREEN_OFFSET:
        position.vx = node->x + node->sprite_offset_x + g_camera_matrix.t[0];
        position.vy = node->y + node->sprite_offset_y + g_camera_matrix.t[1];
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        effect_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, &g_camera_zoom, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_WORLD_UNSCALED:
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&input, node->x + node->sprite_offset_x, node->y + node->sprite_offset_y, node->z);
        RotTrans(&input, &output, &output.pad);
        depth = output.vz >> 2;
        switch (node->depth_mode) {
        case EFFECT_SPRITE_DEPTH_NEARER_8:
            depth -= 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_8:
            depth = 8;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_FARTHEST:
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
            break;
        case EFFECT_SPRITE_DEPTH_FIXED_16:
            depth = 16;
            break;
        case EFFECT_SPRITE_DEPTH_NEARER_16:
            depth -= 16;
            break;
        }
        if (depth < 0) {
            return;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        position.vx = output.vx;
        position.vy = output.vy;
        effect_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, 0, g_effect_otag + depth);
        break;
    case EFFECT_LIST_NODE_SCREEN:
        position.vx = node->x + node->sprite_offset_x;
        position.vy = node->y + node->sprite_offset_y;
        depth = node->z;
        if (depth < 0) {
            depth = 0;
        }
        if (depth > EFFECT_SPRITE_OT_DEPTH_MAX) {
            depth = EFFECT_SPRITE_OT_DEPTH_MAX;
        }
        effect_submit_sprite_to_ordering_table(
            node->sprite_block, &position, node->screen_rotation_angle, 0, g_effect_otag + depth);
        break;
    }
}

/* The two flag meanings remain unknown. */
enum { EFFECT_REPLACED_FLAGS_MASK = 0x0006 };

/* Nothing in the game calls it. */
void effect_replace_flags_0006(effect_flag_prefix_t* object, s32 value) {
    object->flags = (object->flags & ~EFFECT_REPLACED_FLAGS_MASK) | (value & EFFECT_REPLACED_FLAGS_MASK);
}

/* Advance one effect slot (a particle of a secondary effect) by a frame: step its
 * motion, advance its frame script, in which each step shows a frame for hold
 * calls and the last step wraps to the first, and draw it as a billboard.
 *
 * Returns 0 when the slot has finished: a positive life counts down and ends the
 * slot at 0; a negative life ends it on the frame the script wraps. */
s32 effect_update_slot(s32 id) {
    effect_slot_t* slot = &g_effect_slots[id];
    s32 wrapped;
    s32 result;

    effect_step_motion(&slot->motion);
    wrapped = 0;
    if (--slot->hold <= 0) {
        slot->frame = slot->script->step[slot->step_index].frame;
        slot->hold = slot->script->step[slot->step_index].hold;
        slot->step_index++;
        if (slot->step_index >= slot->script->count) {
            wrapped = 1;
            slot->step_index = 0;
        }
    }
    effect_draw_billboard_sprite(slot);
    result = 1;
    if (slot->life < 0) {
        result = wrapped == 0;
    } else if (--slot->life == 0) {
        result = 0;
    }
    return result;
}

/* Four effect-pool/state initializers, called at effect setup and teardown. */
void effect_reset_subsystems(void) {
    effect_init_render_state();
    effect_init_state_records();
    effect_init_record_chain();
    effect_init_free_list();
}

/* Effect vectors and camera setup. Its start is a cut chosen by topic; its end is a boundary the binary proves. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/map.h"
#include "psx/gte_inline_split.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

void battle_noop_801a8e74(void) {
}

/* Select the single object's relative sections and rewind its primitive stream. */
s32 effect_init_model_sections(psyq_tmd_header_t* resource, s32 unused_1) {
    g_effect_model_object = resource->objects;
    /* TMD offsets are byte counts from the object table; word indexing keeps signed rounding. */
    g_effect_model_vertices = (SVECTOR*)&((u32*)resource->objects)[(s32)resource->objects[0].vertex_offset / 4];
    g_effect_model_normals = (SVECTOR*)&((u32*)resource->objects)[(s32)resource->objects[0].normal_offset / 4];
    g_effect_model_commands
        = (psyq_tmd_packet_t*)&((u32*)resource->objects)[(s32)resource->objects[0].primitive_offset / 4];
    g_effect_model_command_word_offset = 0;
    return resource->objects[0].primitive_count;
}

/* Decode the next effect command and advance the command index.
 *
 * Opcodes 0x31 and 0x39 carry three or four colors plus vertex indices; 0x35 and
 * 0x37 add five texture operands. Any other opcode is skipped by its length byte.
 *
 * The 0x35/0x37 stream uses variable-length commands, so its index is a 32-bit-element
 * offset rather than a psyq_tmd_packet_t array index. */
void effect_decode_next_model_command(effect_decoded_model_command_t* out) {
    psyq_tmd_packet_t* command;
    s32 index;
    effect_decoded_model_command_t* dst;
    u32 header;

    dst = out;
    index = g_effect_model_command_word_offset;
    command = (psyq_tmd_packet_t*)((u8*)g_effect_model_commands + index * sizeof(u32));
    header = command->id;
    memcpy(&out->header, &command->id, sizeof(out->header));
    /* Struct assignments use unaligned copies; memcpy preserves the 32-bit transfers.
     * The cached header preserves dispatch load/store order and the unsigned range test. */
    switch (header >> 24) {
    case EFFECT_MODEL_COMMAND_G4:
        memcpy(&out->colors[0], &command->g4.r0, sizeof(out->colors[0]));
        memcpy(&out->colors[1], &command->g4.r1, sizeof(out->colors[1]));
        memcpy(&out->colors[2], &command->g4.r2, sizeof(out->colors[2]));
        memcpy(&out->colors[3], &command->g4.r3, sizeof(out->colors[3]));
        memcpy(&out->vertices.pairs[0], &command->g4.v0, sizeof(out->vertices.pairs[0]));
        /* A 32-bit load preserves the native delay-slot store; memcpy reschedules it. */
        out->vertices.pairs[1] = *(u32*)&command->g4.v2;
        g_effect_model_command_word_offset = index + 7;
        break;
    case EFFECT_MODEL_COMMAND_G3:
        memcpy(&out->colors[0], &command->g3.r0, sizeof(out->colors[0]));
        memcpy(&out->colors[1], &command->g3.r1, sizeof(out->colors[1]));
        memcpy(&out->colors[2], &command->g3.r2, sizeof(out->colors[2]));
        memcpy(&out->vertices.pairs[0], &command->g3.v0, sizeof(out->vertices.pairs[0]));
        /* A 32-bit load preserves the native delay-slot store; memcpy reschedules it. */
        out->vertices.pairs[1] = *(u32*)&command->g3.v2;
        g_effect_model_command_word_offset = index + 6;
        break;
    case EFFECT_MODEL_COMMAND_GT3_35:
    case EFFECT_MODEL_COMMAND_GT3_37: {
        psyq_tmd_packet_t* extended;
        extended
            = (psyq_tmd_packet_t*)((u8*)g_effect_model_commands + g_effect_model_command_word_offset * sizeof(u32));
        memcpy(&dst->colors[0], &extended->gt3.r0, sizeof(dst->colors[0]));
        memcpy(&dst->colors[1], &extended->gt3.r1, sizeof(dst->colors[1]));
        memcpy(&dst->colors[2], &extended->gt3.r2, sizeof(dst->colors[2]));
        memcpy(&dst->vertices.pairs[0], &extended->gt3.v0, sizeof(dst->vertices.pairs[0]));
        memcpy(&dst->vertices.pairs[1], &extended->gt3.v2, sizeof(dst->vertices.pairs[1]));
        out->texture.tpage = extended->gt3.tpage;
        out->texture.clut = extended->gt3.clut;
        memcpy(&out->texture.uv[0], &extended->gt3.tu0, sizeof(out->texture.uv[0]));
        memcpy(&out->texture.uv[1], &extended->gt3.tu1, sizeof(out->texture.uv[1]));
        memcpy(&out->texture.uv[2], &extended->gt3.tu2, sizeof(out->texture.uv[2]));
        g_effect_model_command_word_offset += 9;
        break;
    }
    default:
        g_effect_model_command_word_offset += out->header.word_count + 1;
        break;
    }
}

/* Resolve one effect target to integer world coordinates.
 *
 * A unit target copies the unit's world-coordinate record, and when
 * add_depth is set lowers Y to the tile's own surface height. A tile target
 * uses the tile center (x * 28 + 14, y * 28 + 14) and takes its height
 * from the tile record, adding the slope depth when add_depth is set. */
void effect_resolve_target_world_coords(
    s32 add_depth, s32 target_index, effect_secondary_init_t* coord_data, SVECTOR* out) {
    map_tile_t* tile;

    if (target_index == EFFECT_TARGET_INDEX_CASTER) {
        if (coord_data->targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            *out = *battle_unit_get_world_coords_by_runtime_id(
                coord_data->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
            if (add_depth != 0) {
                s32 height;
                tile = map_get_unit_tile_by_runtime_id(coord_data->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
                height = -((tile->height + tile->depth_half_height.depth) * 12);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = map_get_tile_data_pointer(coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vx,
                coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vz,
                coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vy);
            out->vx = coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            out->vz = coord_data->targets[EFFECT_CASTER_ENTRY].data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            if (add_depth != 0) {
                out->vy = -((tile->height + tile->depth_half_height.depth) * 12);
            } else {
                out->vy = -(tile->height * 12);
            }
        }
    } else {
        if (coord_data->targets[target_index].target_type == EFFECT_TARGET_TYPE_UNIT) {
            *out = *battle_unit_get_world_coords_by_runtime_id(coord_data->targets[target_index].data.unit.runtime_id);
            if (add_depth != 0) {
                s32 height;
                tile = map_get_unit_tile_by_runtime_id(coord_data->targets[target_index].data.unit.runtime_id);
                height = -((tile->height + tile->depth_half_height.depth) * 12);
                if (height < out->vy) {
                    out->vy = height;
                }
            }
        } else {
            tile = map_get_tile_data_pointer(coord_data->targets[target_index].data.tile.vx,
                coord_data->targets[target_index].data.tile.vz, coord_data->targets[target_index].data.tile.vy);
            out->vx = coord_data->targets[target_index].data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            out->vz = coord_data->targets[target_index].data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            if (add_depth != 0) {
                out->vy = -((tile->height + tile->depth_half_height.depth) * 12);
            } else {
                out->vy = -(tile->height * 12);
            }
        }
    }
}

/* Resolve a battle ID before calculating its integer world coordinates. */
void effect_get_world_coords_by_battle_id(
    s32 adjust_for_tile_height, u32 battle_id, effect_secondary_init_t* effect_coordinates, SVECTOR* destination) {
    effect_resolve_target_world_coords(
        adjust_for_tile_height, battle_unit_get_runtime_id_by_battle_id(battle_id), effect_coordinates, destination);
}

void effect_load_vector(const vec3s16_t* src, VECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

void effect_load_svector(const vec3s16_t* src, SVECTOR* dst) {
    dst->vx = src->x;
    dst->vy = src->y;
    dst->vz = src->z;
}

void effect_shift_vector_right_12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx >> 12;
    dst->vy = src->vy >> 12;
    dst->vz = src->vz >> 12;
}

/* Convert integral vector components to 20.12 fixed point. */
void effect_scale_vector_fixed12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx * ONE;
    dst->vy = src->vy * ONE;
    dst->vz = src->vz * ONE;
}

void effect_mask_vector_low_12_bits(const SVECTOR* src, SVECTOR* dst) {
    dst->vx = src->vx & 0xFFF;
    dst->vy = src->vy & 0xFFF;
    dst->vz = src->vz & 0xFFF;
}

void effect_add_vectors(const VECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

void effect_add_svectors(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

void effect_add_svector_to_vector(const SVECTOR* first, const VECTOR* second, VECTOR* out) {
    out->vx = first->vx + second->vx;
    out->vy = first->vy + second->vy;
    out->vz = first->vz + second->vz;
}

void effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}

/* Add random X/Z/Y offsets and convert each result to Q12 coordinates. */
void effect_add_random_vector_offsets_and_store_q12(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = (source->vx + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vy;
    destination->vy = (source->vy + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vz;
    destination->vz = (source->vz + effect_get_random_between_s16(-range, range)) * ONE;
}

void effect_add_vector_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out) {
    value->vx += delta->vx;
    out->vx = value->vx * ONE;
    value->vy += delta->vy;
    out->vy = value->vy * ONE;
    value->vz += delta->vz;
    out->vz = value->vz * ONE;
}

/* Add independent random offsets to three packed rotation components. */
void effect_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}

enum {
    BATTLE_ROTATION_Q12_MASK = 0x0FFF,
};

/* Add two effect rotations and wrap each axis to one Q12 revolution. */
void effect_add_rotation_vectors_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = (first->vx + second->vx) & BATTLE_ROTATION_Q12_MASK;
    out->vy = (first->vy + second->vy) & BATTLE_ROTATION_Q12_MASK;
    out->vz = (first->vz + second->vz) & BATTLE_ROTATION_Q12_MASK;
}

/* Nothing in the game calls this or its three siblings for 4, 6 and 8. */
void effect_add_2_to_halfword(u16* value) {
    *value += 2;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_4_to_halfword(u16* value) {
    *value += 4;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_6_to_halfword(u16* value) {
    *value += 6;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_8_to_halfword(u16* value) {
    *value += 8;
}

void effect_add_vectors_to_svector(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vz;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 270 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x - rz, y + ry, z + rx). */
void effect_add_vectors_to_svector_rot_y_270(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz + right->vx;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 180 degrees about Y, i.e. (x - rx, y + ry, z - rz). */
void effect_add_vectors_to_svector_rot_y_180(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx - right->vx;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vz;
}

/* Only the low 16 bits of each input component are read. */
/* result = left + right rotated 90 degrees about Y (RotMatrixY convention:
 * x' = x cos + z sin, z' = -x sin + z cos), i.e. (x + rz, y + ry, z - rx). */
void effect_add_vectors_to_svector_rot_y_90(const VECTOR* left, const VECTOR* right, SVECTOR* result) {
    result->vx = left->vx + right->vz;
    result->vy = left->vy + right->vy;
    result->vz = left->vz - right->vx;
}

void effect_set_gravity(const vec3s32_t* src) {
    g_effect_gravity = *src;
}

void effect_set_inertia_threshold(s32 threshold) {
    g_effect_inertia_threshold = threshold;
}

void effect_save_gravity(void) {
    g_effect_saved_gravity = g_effect_gravity;
}

void effect_restore_gravity(void) {
    g_effect_gravity = g_effect_saved_gravity;
}

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

void effect_copy_camera_angles_and_world_coords(void) {
    SVECTOR* rotation;
    VECTOR* coords;

    rotation = camera_get_rotation();
    g_effect_camera_rotation_saved.vx = g_effect_camera_rotation_start.vx = g_effect_camera_rotation_target.vx
        = g_effect_camera_rotation_current.vx = rotation->vx & 0xfff;
    g_effect_camera_rotation_saved.vy = g_effect_camera_rotation_start.vy = g_effect_camera_rotation_target.vy
        = g_effect_camera_rotation_current.vy = rotation->vy & 0xfff;
    g_effect_camera_rotation_saved.vz = g_effect_camera_rotation_start.vz = g_effect_camera_rotation_target.vz
        = g_effect_camera_rotation_current.vz = rotation->vz & 0xfff;
    coords = camera_get_current_real_coords();
    g_effect_camera_position_saved.vx = g_effect_camera_position_start.vx = g_effect_camera_position_target.vx
        = g_effect_camera_position_current.vx = coords->vx >> 12;
    g_effect_camera_position_saved.vy = g_effect_camera_position_start.vy = g_effect_camera_position_target.vy
        = g_effect_camera_position_current.vy = coords->vy >> 12;
    g_effect_camera_position_saved.vz = g_effect_camera_position_start.vz = g_effect_camera_position_target.vz
        = g_effect_camera_position_current.vz = coords->vz >> 12;
    g_effect_camera_zoom_saved = g_effect_camera_zoom_start = g_effect_camera_zoom_target = g_effect_camera_zoom_current
        = *camera_get_zoom();
}

void effect_init_camera_and_screen_data(void) {
    s32 pad[2];
    g_effect_camera_rotation_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_position_mode = EFFECT_CAMERA_MODE_IDLE;
    g_effect_camera_zoom_mode = EFFECT_CAMERA_MODE_IDLE;
    g_battle_unknown_801b8b1c = 0;
    g_battle_unknown_801b8b18 = 0;
    effect_copy_camera_angles_and_world_coords();
    (void)pad;
}

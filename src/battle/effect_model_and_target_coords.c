#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/libc.h"
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

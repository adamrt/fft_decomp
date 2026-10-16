/* The Banish particle handler, spawning each active emitter's particles at cached map mesh vertices. */

#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/effect_geometry.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Banish particle handler: phase 1 allocates the 32-vertex cache and sets map
 * 3D objects 1-6 to state 1, phase 2 spawns each live emitter's particles at
 * its cached vertices, phase 3 frees the cache.
 *
 * The cache holds four vertices of each of map mesh parts 1-8, which the map
 * renderer keeps current while g_effect_map_vertices is set. The
 * overlay table maps emitter vertex groups 1-12 to one vertex each; groups 13
 * and 14 spawn at every vertex of groups 1-4 and 5-12. An emitter is live
 * from its start_frame up to, not including, its end_frame. */
s32 effect_banish_map_vertex_particles_update(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_effect_state_records[state_index];
    /* VECTOR indexing changes the offset-sum register. */
    u8* work;
    effect_work_record_t parent;
    s32 emitter_index;
    s32 group;
    s32 i;
    s32 byte_offset;
    const effect_particle_vertex_emitter_view_t* emitter;
    VECTOR* vertex;
    VECTOR* vertex_work;

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        vertex_work = effect_heap_alloc_block(EFFECT_MAP_VERTEX_WORK_BYTES, state_index);
        record->work_slots[callback_slot] = vertex_work;
        g_effect_map_vertices = vertex_work;
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 1, 1);
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 1, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        for (emitter_index = 0; emitter_index < EFFECT_PARTICLE_VERTEX_EMITTER_COUNT; emitter_index++) {
            emitter = &g_effect_geometry_table->entries[emitter_index].particle_vertex_emitter;
            group = emitter->vertex_group;
            if (group >= 1 && group <= 12) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    i = group - 1;
                    byte_offset = g_effect_banish_vertex_locations[i].vertex_index * sizeof(*vertex);
                    byte_offset += g_effect_banish_vertex_locations[i].mesh_part_index
                        * (EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(*vertex));
                    vertex = (VECTOR*)(work + byte_offset);
                    parent.motion.position.x = vertex->vx * ONE;
                    parent.motion.position.y = vertex->vy * ONE;
                    parent.motion.position.z = vertex->vz * ONE;
                    effect_particle_spawn_from_emitter(
                        record->record_index, record->frame_counter - emitter->start_frame, emitter_index, &parent);
                }
            } else if (group == 13) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    for (i = 0; i < 4; i++) {
                        byte_offset = g_effect_banish_vertex_locations[i].vertex_index * sizeof(*vertex);
                        byte_offset += g_effect_banish_vertex_locations[i].mesh_part_index
                            * (EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(*vertex));
                        vertex = (VECTOR*)(work + byte_offset);
                        parent.motion.position.x = vertex->vx * ONE;
                        parent.motion.position.y = vertex->vy * ONE;
                        parent.motion.position.z = vertex->vz * ONE;
                        effect_particle_spawn_from_emitter(
                            record->record_index, record->frame_counter - emitter->start_frame, emitter_index, &parent);
                    }
                }
            } else if (group == 14) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    for (i = 4; i < 12; i++) {
                        byte_offset = g_effect_banish_vertex_locations[i].vertex_index * sizeof(*vertex);
                        byte_offset += g_effect_banish_vertex_locations[i].mesh_part_index
                            * (EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(*vertex));
                        vertex = (VECTOR*)(work + byte_offset);
                        parent.motion.position.x = vertex->vx * ONE;
                        parent.motion.position.y = vertex->vy * ONE;
                        parent.motion.position.z = vertex->vz * ONE;
                        effect_particle_spawn_from_emitter(
                            record->record_index, record->frame_counter - emitter->start_frame, emitter_index, &parent);
                    }
                }
            }
        }
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
            record->work_slots[callback_slot] = 0;
            g_effect_map_vertices = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}

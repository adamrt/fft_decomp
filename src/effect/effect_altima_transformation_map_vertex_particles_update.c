/* The Altima transformation particle handler, spawning particles at map vertices for timed emitters. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_map.h"
#include "fft/effect.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Altima transformation particle handler: allocate the map-vertex work,
 * enable map data groups 1-6, then spawn particles for timed vertex emitters.
 * E464's shipped emitter records all have vertex group 0, so none spawn here. */
s32 effect_altima_transformation_map_vertex_particles_update(
    s32 record_index, s32 callback_slot, s32 unused_emitter_index, s32 frame) {
    s16 state_index = record_index;
    effect_record_t* record;
    VECTOR* work;
    effect_work_record_t parent;
    s32 emitter_index;
    s32 i;
    s32 byte_offset;
    const effect_particle_vertex_emitter_view_t* emitter;
    VECTOR* vertex;
    VECTOR* vertex_work;

    record = &g_battle_effect_state_records[state_index];

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;
    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        vertex_work = battle_heap_alloc_block(EFFECT_MAP_VERTEX_WORK_BYTES, state_index);
        record->work_slots[callback_slot] = vertex_work;
        g_battle_effect_map_vertices = vertex_work;
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 1, 1);
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;
    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        for (emitter_index = 0; emitter_index < EFFECT_PARTICLE_VERTEX_EMITTER_COUNT; emitter_index++) {
            emitter = &g_battle_effect_geometry_table->entries[emitter_index].particle_vertex_emitter;
            i = emitter->vertex_group - 1;
            if (i >= 0 && i < 12 && frame >= emitter->start_frame && frame < emitter->end_frame) {
                /* Byte accumulation preserves the vertex and mesh-part load order. */
                byte_offset = g_effect_altima_transformation_vertex_locations[i].vertex_index * sizeof(*work);
                byte_offset += g_effect_altima_transformation_vertex_locations[i].mesh_part_index
                    * (EFFECT_MAP_VERTEX_COUNT_PER_PART * sizeof(*work));
                vertex = &work[byte_offset / sizeof(*work)];
                parent.motion.position.x = vertex->vx * ONE;
                parent.motion.position.y = vertex->vy * ONE;
                parent.motion.position.z = vertex->vz * ONE;
                battle_effect_spawn_emitter_particles(
                    record->record_index, record->frame_counter - emitter->start_frame, emitter_index, &parent);
            }
        }
        break;
    case EFFECT_CALLBACK_PHASE_DESTROY:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
            record->work_slots[callback_slot] = 0;
            g_battle_effect_map_vertices = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}

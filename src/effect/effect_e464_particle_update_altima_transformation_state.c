#include "fft/effect.h"

/* E464: Altima's Transformation; the target range ends before trailing data. */

#define NULL ((void*)0)

/* Altima transformation particle handler, the single-group form of
 * effect_e454_particle_update_banish_state: phase 1 allocates the 0x200-byte
 * vertex work and enables map data groups 1-6, phase 2 spawns each active
 * emitter's particles at its work vertex, phase 3 frees the work. */
s32 effect_e464_particle_update_altima_transformation_state(
    s16 record_index, s32 byte_offset, s32 entry_index, s32 frame) {
    effect_record_t* record;
    u8* work;
    effect_work_record_t parent;
    s32 emitter_index;
    s32 i;
    s32 offset;
    effect_particle_vertex_emitter_view_t* emitter;
    VECTOR* vertex;
    u8* vertex_work;

    record = &g_effect_state_records[record_index];

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != NULL) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        vertex_work = battle_heap_alloc_block(0x200, record_index);
        record->work_slots[byte_offset] = vertex_work;
        g_current_effect_work = vertex_work;
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 1, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 2, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 3, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 4, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 5, 1, 1);
        battle_map_dispatch_map_data_command(MAP_DATA_COMMAND_SET_3D_OBJECT_STATE, 6, 1, 1);
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        work = record->work_slots[byte_offset];
        emitter_index = 0;
        do {
            emitter = (effect_particle_vertex_emitter_view_t*)&g_effect_geometry_table->entries[emitter_index];
            i = emitter->vertex_group - 1;
            if ((u32)i < 12 && frame >= emitter->start_frame && frame < emitter->end_frame) {
                offset = (&g_effect_e464_particle_work_offset_scale_16)[i * 2] << 4;
                offset += (&g_effect_e464_particle_work_offset_scale_64)[i * 2] << 6;
                vertex = (VECTOR*)(work + offset);
                parent.position[0] = vertex->vx << 12;
                parent.position[1] = vertex->vy << 12;
                parent.position[2] = vertex->vz << 12;
                battle_effect_spawn_emitter_particles(
                    record->record_index, record->emitter_index - emitter->start_frame, emitter_index, &parent);
            }
            emitter_index++;
        } while (emitter_index < 16);
        break;
    case EFFECT_PHASE_DESTROY:
        if (record->work_slots[byte_offset] != NULL) {
            battle_heap_free_block(record->work_slots[byte_offset]);
            record->work_slots[byte_offset] = NULL;
            g_current_effect_work = NULL;
        }
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}

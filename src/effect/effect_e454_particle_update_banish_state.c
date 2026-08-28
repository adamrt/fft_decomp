#include "fft/effect.h"
#include "fft/map.h"

#define NULL ((void*)0)

/* This handler's view of effect_geometry_entry_t: 0x4c selects the work
 * vertices that seed the emitter and 0xa8/0xaa bound its active frames. */
typedef struct effect_banish_emitter_view {
    u8 _unknown_00[0x4c];
    s16 vertex_group; /* 0x4c: 1-12 one vertex, 13 row 0, 14 rows 1-4 */
    u8 _unknown_4e[0x5a];
    s16 start_frame; /* 0xa8 */
    s16 end_frame;   /* 0xaa */
    u8 _unknown_ac[0x18];
} effect_banish_emitter_view_t;

/* Banish particle handler: phase 1 allocates the 0x200-byte vertex work and
 * enables map data groups 1-6, phase 2 spawns each active emitter's particles
 * at its work vertices, phase 3 frees the work.
 *
 * The work holds four 16-byte vertex slots per 64-byte row; the overlay table
 * at 0x801c48dc lists (slot, row) byte pairs for emitter groups 1-12, and
 * groups 13 and 14 walk pairs 0-3 and 4-11. The loops step the byte index by
 * 2, which keeps loop.c from reducing the table address to a pointer. */
s32 effect_e454_particle_update_banish_state(s16 record_index, s32 byte_offset, s32 entry_index, s32 frame) {
    effect_record_t* record;
    u8* work;
    effect_work_record_t parent;
    s32 emitter_index;
    s32 group;
    s32 i;
    s32 offset;
    effect_banish_emitter_view_t* emitter;
    VECTOR* vertex;
    void* new_work;

    record = &g_effect_state_records[record_index];

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != NULL) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        new_work = battle_heap_alloc_block(0x200, record_index);
        record->work_slots[byte_offset] = new_work;
        g_current_effect_work = new_work;
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
            emitter = (effect_banish_emitter_view_t*)&g_effect_geometry_table->entries[emitter_index];
            group = emitter->vertex_group;
            if ((u32)(group - 1) < 12) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    i = group - 1;
                    offset = (&g_effect_e454_particle_work_offset_scale_16)[i * 2] << 4;
                    offset += (&g_effect_e454_particle_work_offset_scale_64)[i * 2] << 6;
                    vertex = (VECTOR*)(work + offset);
                    parent.position[0] = vertex->vx << 12;
                    parent.position[1] = vertex->vy << 12;
                    parent.position[2] = vertex->vz << 12;
                    battle_effect_spawn_emitter_particles(
                        record->record_index, record->emitter_index - emitter->start_frame, emitter_index, &parent);
                }
            } else if (group == 13) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    for (i = 0; i < 8; i += 2) {
                        offset = (&g_effect_e454_particle_work_offset_scale_16)[i] << 4;
                        offset += (&g_effect_e454_particle_work_offset_scale_64)[i] << 6;
                        vertex = (VECTOR*)(work + offset);
                        parent.position[0] = vertex->vx << 12;
                        parent.position[1] = vertex->vy << 12;
                        parent.position[2] = vertex->vz << 12;
                        battle_effect_spawn_emitter_particles(
                            record->record_index, record->emitter_index - emitter->start_frame, emitter_index, &parent);
                    }
                }
            } else if (group == 14) {
                if (frame >= emitter->start_frame && frame < emitter->end_frame) {
                    for (i = 8; i < 24; i += 2) {
                        offset = (&g_effect_e454_particle_work_offset_scale_16)[i] << 4;
                        offset += (&g_effect_e454_particle_work_offset_scale_64)[i] << 6;
                        vertex = (VECTOR*)(work + offset);
                        parent.position[0] = vertex->vx << 12;
                        parent.position[1] = vertex->vy << 12;
                        parent.position[2] = vertex->vz << 12;
                        battle_effect_spawn_emitter_particles(
                            record->record_index, record->emitter_index - emitter->start_frame, emitter_index, &parent);
                    }
                }
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

#include "fft/map.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Loads one record of the saved map state and returns the address just past it.
 *
 * Type 1 fills entries 1..count of the 0x50-byte table, type 2 the 16 cells of
 * each of 8 rows for entries 1..8 of the 0x220-byte table, and type 3 sets up
 * the 0x98-byte entries 1..8 from the other two tables (4 input bytes per
 * entry, extra entries skipped). The caller at 0x800f32e4 stops at type 0
 * before calling, so the result for type 0 or an unknown type is never used;
 * `next` is left unset there, which is why the target returns $a1. */
u8* battle_map_load_saved_state_record(battle_map_state_record_t* record) {
    u8* next;
    u32 count;
    u32 i;
    u32 y;
    u32 c;

    count = record->count;
    switch (record->type) {
    case 1: {
        u8* src = record->data;

        count++;
        for (i = 1; i < count; i++) {
            g_battle_map_mesh_animation_keyframes[i] = *(battle_map_mesh_keyframe_t*)src;
            src += sizeof(battle_map_mesh_keyframe_t);
        }
        next = src;
        break;
    }
    case 0:
        break;
    case 2: {
        u8* src = record->data;

        for (y = 0; y < 8; y++) {
            for (i = 1; i < 9; i++) {
                for (c = 1; c < 17; c++) {
                    g_battle_map_mesh_animation_instructions[i].states[y][c] = *(battle_map_mesh_instruction_t*)src;
                    src += sizeof(battle_map_mesh_instruction_t);
                }
            }
        }
        next = src;
        break;
    }
    case 3: {
        u8* src = record->data;

        count++;
        for (i = 1; i < count; i++) {
            if (i >= 9) {
                break;
            }
            g_battle_map_mesh_parts[i].parent = *src;
            g_battle_map_mesh_parts[i].value_82 = 0;
            g_battle_map_mesh_parts[i].value_80 = 2;
            src += 4;
            if (g_battle_map_mesh_animation_instructions[i].states[0][1].next != 0) {
                g_battle_map_mesh_part_animation_states[i] = 1;
            } else {
                g_battle_map_mesh_part_animation_states[i] = 0;
            }
            g_battle_map_mesh_parts[i].value_87 = 0;
            g_battle_map_mesh_parts[i].vectors_a[1]
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .vector_0;
            g_battle_map_mesh_parts[i].vectors_b[1]
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .vector_8;
            g_battle_map_mesh_parts[i].vectors_c[1].vx
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .value_10;
            g_battle_map_mesh_parts[i].vectors_c[1].vy
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .value_12;
            g_battle_map_mesh_parts[i].vectors_c[1].vz
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .value_14;
        }
        for (; i < count; i++) {
            src += 4;
        }
        next = src;
        break;
    }
    }
    return next;
}

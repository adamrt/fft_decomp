#include "fft/battle.h"
#include "fft/main_heap.h"
#include "fft/script_variables.h"

typedef struct packed_gns_record_t {
    u8 bytes[0x14];
} packed_gns_record_t;

enum {
    MAP_MESH_LOAD_IDLE = 0,
    MAP_MESH_LOAD_WAITING = 0x8d,
};

extern packed_gns_record_t g_battle_map_selected_mesh_record;

/* Target 0x800f4acc selects the last primary-mesh record, then substitutes
 * an alternate whose layout/weather/time comparison matches. It copies the
 * selected 20-byte record verbatim, then uses processed load information at
 * +0x06 to load into a temporary 0x20000-byte buffer. Its loader's fourth
 * argument is 0x30; that argument is forwarded as display suppression, not
 * used to index a resource slot by battle_map_start_file_load.
 * The disk GNS record and processed loading view must not be conflated. */
s32 battle_map_load_mesh_variant(s32 mesh_slot) {
    switch (g_battle_map_mesh_load_state) {
    case MAP_MESH_LOAD_IDLE:
        if (g_battle_map_first_gns_resource_type < 0x80) {
            {
                u8* record_data;
                u8* resource_type;
                packed_gns_record_t* record;
                packed_gns_record_t* selected_record;
                /* Pin: unpinned, GCC hoists the 0x2e compare constant into
                 * $t0 instead of $t1 (register swap only, same size). */
                register s32 primary_resource_type asm("$9");

                selected_record = &g_battle_map_selected_mesh_record;
                primary_resource_type = GNS_RESOURCE_MESH_PRIMARY;
                /* One byte base feeds both cursors; separate typed bases do not match. */
                record_data = (u8*)&g_battle_map_gns_records[0];
                resource_type = record_data + 5;
                record = (packed_gns_record_t*)record_data;
                do {
                    if (*resource_type == primary_resource_type) {
                        *selected_record = *record;
                    }
                    resource_type += sizeof(packed_gns_record_t);
                    record++;
                } while (*resource_type < 0x80);
            }

            if (g_battle_map_first_gns_resource_type < 0x80) {
                u8* resource_type;
                s32 record_offset;
                s32 map_state;
                s32 map_state_and_weather;

                /* The target re-derives each field from the table base plus a
                 * byte offset; a typed record cursor or index does not match. */
                resource_type = &g_battle_map_gns_records[0].resource_type;
                record_offset = 0;
                do {
                    if (*resource_type == GNS_RESOURCE_MESH_ALTERNATE) {
                        map_state
                            = battle_script_get_variable(*(s16*)((u8*)&g_battle_map_gns_records[0] + record_offset))
                            & 0xfff;
                        map_state_and_weather
                            = map_state | ((battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12);
                        if ((battle_map_is_gns_record_matching_state(
                                 *((u8*)&g_battle_map_gns_records[0] + record_offset + 4),
                                 *(s16*)((u8*)&g_battle_map_gns_records[0] + record_offset + 2),
                                 map_state_and_weather
                                     | ((battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15))
                                << 16)
                            != 0) {
                            g_battle_map_selected_mesh_record
                                = *(packed_gns_record_t*)((u8*)&g_battle_map_gns_records[0] + record_offset);
                        }
                    }
                    resource_type += sizeof(packed_gns_record_t);
                    record_offset += sizeof(packed_gns_record_t);
                } while (*resource_type < 0x80);
            }
        }

        g_battle_map_mesh_load_buffer = game_malloc(0x20000);
        if (g_battle_map_mesh_load_buffer == 0) {
            main_system_handle_malloc_exception(2, 0x3c9);
        }
        battle_map_start_file_load(g_battle_map_file_table, &g_battle_map_selected_mesh_record.bytes[6],
            (s32)g_battle_map_mesh_load_buffer, 0x30);
        g_battle_map_mesh_load_state = MAP_MESH_LOAD_WAITING;
        break;

    case MAP_MESH_LOAD_WAITING:
        if (main_file_is_still_loading() != 0) {
            s32 mesh_data_offset;
            void* mesh_data;

            mesh_data = g_battle_map_mesh_load_buffer;
            mesh_data_offset = ((map_mesh_file_header_t*)mesh_data)->palette_word_offset * 4;
            mesh_data = (void*)((s32)mesh_data + mesh_data_offset);
            battle_map_update_palette_colors(mesh_slot, 1, 0, 1, (const u16*)mesh_data);
            main_heap_free(g_battle_map_mesh_load_buffer);
            g_battle_map_mesh_load_state = MAP_MESH_LOAD_IDLE;
        }
        break;
    }
    return g_battle_map_mesh_load_state;
}

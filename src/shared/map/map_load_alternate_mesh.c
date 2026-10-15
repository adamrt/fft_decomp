#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"

enum {
    MAP_MESH_LOAD_IDLE = 0,
    MAP_MESH_LOAD_WAITING = 0x8d,
};

/* Selects the last primary-mesh record, then substitutes an alternate whose
 * layout/weather/time comparison matches, and loads the selected record's
 * file into a temporary 0x20000-byte buffer. The loader's fourth argument is
 * the resource type, as at its other call sites; map_start_file_load
 * does not read it. `palette_mode` is the animation mode the alternate mesh's
 * palette is applied with (map_update_palette_colors). */
s32 map_load_alternate_mesh(s32 palette_mode) {
    s32 i;
    s16 state;

    switch (g_map_mesh_load_state) {
    case MAP_MESH_LOAD_IDLE:
        for (i = 0; g_map_gns_records[i].resource_type < GNS_RECORD_COMMAND_SEPARATOR; i++) {
            if (g_map_gns_records[i].resource_type == MAP_RESOURCE_MESH_PRIMARY) {
                g_map_selected_mesh_record = g_map_gns_records[i];
            }
        }
        for (i = 0; g_map_gns_records[i].resource_type < GNS_RECORD_COMMAND_SEPARATOR; i++) {
            if (g_map_gns_records[i].resource_type == MAP_RESOURCE_MESH_ALTERNATE) {
                state = script_get_variable(g_map_gns_records[i].script_variable_id) & 0xfff;
                state |= (script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                state |= (script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                if (map_is_gns_record_matching_state(
                        g_map_gns_records[i].map_state_comparison, g_map_gns_records[i].map_state, state)) {
                    g_map_selected_mesh_record = g_map_gns_records[i];
                }
            }
        }
        g_map_mesh_load_buffer = main_heap_alloc(0x20000);
        if (g_map_mesh_load_buffer == 0) {
            main_system_handle_malloc_exception(2, 0x3c9);
        }
        map_start_file_load(g_map_directory_path, &g_map_selected_mesh_record.load_info, g_map_mesh_load_buffer,
            MAP_RESOURCE_MESH_ALTERNATE);
        g_map_mesh_load_state = MAP_MESH_LOAD_WAITING;
        break;

    case MAP_MESH_LOAD_WAITING:
        if (main_file_is_still_loading() != 0) {
            s32 mesh_data_offset;
            map_resource_t* mesh_data;

            mesh_data = g_map_mesh_load_buffer;
            /* Retail scales this byte offset as a 32-bit-element index. */
            mesh_data_offset = mesh_data->mesh_file[MAP_RESOURCE_PALETTES] * 4;
            mesh_data = (map_resource_t*)(mesh_data->bytes + mesh_data_offset);
            map_update_palette_colors(palette_mode, MAP_CLUT_ROW_MARKED_TILES, 0, 1, mesh_data->clut);
            main_heap_free(g_map_mesh_load_buffer);
            g_map_mesh_load_state = MAP_MESH_LOAD_IDLE;
        }
        break;
    }
    return g_map_mesh_load_state;
}

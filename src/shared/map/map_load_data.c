#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/types.h"

/* Map loader: takes a command between loads, then advances the load of a map's
 * files by one step per call.
 *
 * Commands, read only while g_map_gns_load_phase is idle: START and
 * START_HELD allocate the 0x20000-byte load buffer, read the map's GNS (and reset
 * the mesh state) only when map_id is not the loaded map, and go on to choose
 * resources. LOAD_GNS always reads the GNS; no caller issues it. Any other
 * command, and every call during a load, only advances.
 *
 * Steps:
 * - GNS file: once read, its first 150 rows are copied to
 *   g_map_gns_records.
 * - Selection: fill g_map_pending_gns_records, indexed by resource type.
 *   A row's comparison is tested against the low 12 bits of its script
 *   variable with the weather in bits 12-14 and the time of day in bit 15. A row
 *   is taken when its file bytes differ from the row last loaded for its type.
 *   Four passes: primary meshes, override meshes, the default texture, then
 *   every other row whose comparison holds.
 * - Resources: read the pending rows one file at a time in type order and hand
 *   each to map_dispatch_gns_resource. Types 0x12-0x17 come last: 0x12
 *   raises exception 0x3cb, the texture quarters load without being applied
 *   here, and the texture is followed by the pending lighting, the texture
 *   upload and the pending palettes.
 * - Mesh records: map_append_mesh_geometry requests them; the render
 *   records of the nine mesh parts are rebuilt once in each render buffer.
 * - End: run the command records after the file rows, apply the script
 *   weather, free the buffer.
 *
 * Returns g_map_gns_load_phase: nonzero while loading, 0 when done or
 * idle. A command that cannot start because a file read is in progress also
 * returns 0, and a loaded file held by START_HELD returns MAP_LOAD_RESULT_HELD
 * until MAP_DATA_COMMAND_RELEASE_GNS_HOLD.
 *
 * Texture rows are never copied to g_map_loaded_gns_records, so a selected
 * texture is read again on every start. The fourth pass does not exclude the
 * filler rows (type 0x31): one whose comparison holds reads and writes entry 49
 * of the two 49-entry tables. Quarter n loads at buffer + 0x8000 * (n + 1), one
 * quarter above where the dispatcher uploads it from, and quarter 3 lies past the
 * end of the buffer; retail GNS files hold no quarter rows. */
s32 map_load_data(s32 map_id, s32 command) {
    s32 i;
    s32 r;
    s32 j;
    s16 state;
    u8 diff;

    /* A command is taken only between loads. */
    if (g_map_gns_load_phase == MAP_GNS_LOAD_PHASE_IDLE) {
        switch (command) {
        case MAP_LOAD_COMMAND_LOAD_GNS:
            if (main_file_is_still_loading() != 0) {
                return 0;
            }
            g_map_data_load_buffer = main_heap_alloc(0x20000);
            if (g_map_data_load_buffer == 0) {
                main_system_handle_malloc_exception(2, 0x3c2);
            }
            map_load_gns_and_move_find_items(map_id & 0xffff);
            g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
            g_map_loaded_id = map_id;
            g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
            map_reset_mesh_render_state();
            break;
        case MAP_LOAD_COMMAND_START_HELD:
            g_map_gns_dispatch_held = 1;
        case MAP_LOAD_COMMAND_START:
            if (main_file_is_still_loading() != 0) {
                return 0;
            }
            g_map_data_load_buffer = main_heap_alloc(0x20000);
            if (g_map_data_load_buffer == 0) {
                main_system_handle_malloc_exception(2, 0x3c2);
            }
            g_map_texture_quarter_0_pending = 0;
            g_map_texture_quarter_1_pending = 0;
            g_map_texture_quarter_2_pending = 0;
            g_map_texture_quarter_3_pending = 0;
            g_map_texture_pending = 0;
            g_map_palette_pending = 0;
            g_map_lighting_pending = 0;
            if (map_id != g_map_loaded_id) {
                if (map_load_gns_and_move_find_items(map_id & 0xffff) != g_map_gns_records) {
                    return 0;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
                g_map_loaded_id = map_id;
                g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
                map_reset_mesh_render_state();
            } else {
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
                g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
            }
            break;
        case MAP_LOAD_STATE_GNS_FILE:
        case MAP_LOAD_COMMAND_POLL_HELD:
            break;
        }
    }

    if (g_map_resource_load_state == MAP_LOAD_STATE_IDLE) {
        if (g_map_gns_load_phase != MAP_GNS_LOAD_PHASE_RESOURCES) {
            /* Selection: fill the pending table, indexed by resource type. */
            if (g_map_gns_load_phase == MAP_GNS_LOAD_PHASE_START) {
                g_map_gns_record_cursor = 0;
                g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_RESOURCES;
                {
                    s32 k;

                    for (k = 0; k < GNS_RESOURCE_TABLE_COUNT; k++) {
                        g_map_pending_gns_records[k].resource_type = GNS_RECORD_NONE;
                    }
                }
                /* Pass 1: primary meshes. A row whose comparison holds forgets
                 * what is loaded for its type, so it is always read again. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_MESH_PRIMARY) {
                        state = script_get_variable(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (map_is_gns_record_matching_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            /* No braces: a braced body's block note lengthens this loop by one
                             * luid, and loop.c then hoists the 0x48 out of the record loop. */
                            j = 0;
                            do
                                g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[j] = 0x48;
                            while (++j < 14);
                        }
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                    ^ g_map_gns_records[r].load_info.bytes[k];
                            }
                        }
                        if (diff) {
                            g_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }
                /* Pass 2: override meshes, by the same rule. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_MESH_OVERRIDE) {
                        state = script_get_variable(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (map_is_gns_record_matching_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            j = 0;
                            do
                                g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[j] = 0x48;
                            while (++j < 14);
                        }
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                    ^ g_map_gns_records[r].load_info.bytes[k];
                            }
                        }
                        if (diff) {
                            g_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }
                /* Pass 3: the default texture (state 0 of the two map variables). */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_TEXTURE
                        && (g_map_gns_records[r].script_variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME
                            || g_map_gns_records[r].script_variable_id == EVENT_SCRIPT_VAR_MAP_ARRANGEMENT)
                        && g_map_gns_records[r].map_state == 0) {
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                    ^ g_map_gns_records[r].load_info.bytes[k];
                            }
                        }
                        if (diff) {
                            g_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }
                /* Pass 4: every other row whose comparison holds replaces the
                 * choice for its type, or cancels it when that file is loaded. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type != GNS_RECORD_NONE
                        && (g_map_gns_records[r].script_variable_id != EVENT_SCRIPT_VAR_WEATHER_AND_TIME
                            || g_map_gns_records[r].map_state != 0)
                        && (g_map_gns_records[r].script_variable_id != EVENT_SCRIPT_VAR_MAP_ARRANGEMENT
                            || g_map_gns_records[r].map_state != 0)) {
                        state = script_get_variable(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (map_is_gns_record_matching_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            {
                                s32 k;

                                diff = 0;
                                for (k = 0; k < 14; k++) {
                                    diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type]
                                                .load_info.bytes[k]
                                        ^ g_map_gns_records[r].load_info.bytes[k];
                                }
                            }
                            if (diff) {
                                g_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                            } else {
                                g_map_pending_gns_records[g_map_gns_records[r].resource_type].resource_type
                                    = GNS_RECORD_NONE;
                            }
                        }
                    }
                }
            }
        } else {
            /* Resources: start the next pending file. Types 0x12-0x17 are
             * passed over here and taken after the others. */
            if (main_file_is_still_loading() == 0) {
                while ((g_map_pending_gns_records[g_map_gns_record_cursor].resource_type == GNS_RECORD_NONE
                           && g_map_gns_record_cursor < GNS_RESOURCE_TABLE_COUNT)
                    || (g_map_pending_gns_records[g_map_gns_record_cursor].resource_type >= 0x12
                        && g_map_pending_gns_records[g_map_gns_record_cursor].resource_type <= 0x17)) {
                    g_map_gns_record_cursor++;
                }
                if (g_map_gns_record_cursor < GNS_RESOURCE_TABLE_COUNT) {
                    if (g_map_gns_record_cursor == MAP_RESOURCE_TEXTURE_ANIMATIONS) {
                        bzero(g_map_data_load_buffer, 0x280);
                    }
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[g_map_gns_record_cursor].load_info, g_map_data_load_buffer,
                        g_map_gns_record_cursor);
                    g_map_resource_load_state = MAP_LOAD_STATE_RESOURCE_FILE;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type != GNS_RECORD_NONE) {
                    main_system_handle_malloc_exception(2, 0x3cb);
                    g_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type = GNS_RECORD_NONE;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type = GNS_RECORD_NONE;
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].load_info,
                        g_map_data_load_buffer->bytes + 0x8000, MAP_RESOURCE_TEXTURE_QUARTER_0);
                    g_map_texture_quarter_0_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type = GNS_RECORD_NONE;
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].load_info,
                        g_map_data_load_buffer->bytes + 0x10000, MAP_RESOURCE_TEXTURE_QUARTER_1);
                    g_map_texture_quarter_1_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type = GNS_RECORD_NONE;
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].load_info,
                        g_map_data_load_buffer->bytes + 0x18000, MAP_RESOURCE_TEXTURE_QUARTER_2);
                    g_map_texture_quarter_2_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type = GNS_RECORD_NONE;
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].load_info,
                        g_map_data_load_buffer->bytes + 0x20000, MAP_RESOURCE_TEXTURE_QUARTER_3);
                    g_map_texture_quarter_3_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type = GNS_RECORD_NONE;
                    map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE].load_info, g_map_data_load_buffer,
                        MAP_RESOURCE_TEXTURE);
                    g_map_texture_quarter_0_pending = 1;
                    g_map_texture_quarter_1_pending = 1;
                    g_map_texture_quarter_2_pending = 1;
                    g_map_texture_quarter_3_pending = 1;
                    g_map_texture_pending = 1;
                    g_map_resource_load_state = MAP_LOAD_STATE_APPLY_LIGHTING;
                } else {
                    /* Nothing left: run the command records that follow the
                     * row ending the file rows, then finish. */
                    for (i = 0; g_map_gns_records[i].resource_type < GNS_RECORD_COMMAND_SEPARATOR; i++) {
                        if (g_map_gns_records[i].resource_type == GNS_RECORD_NONE) {
                            break;
                        }
                    }
                    i++;
                    g_map_gns_record_cursor = (s32)&g_map_gns_records[i];
                    while (((map_resource_t*)g_map_gns_record_cursor)->command.resource_type != GNS_RECORD_NONE) {
                        map_apply_gns_command(2, (map_resource_t*)g_map_gns_record_cursor);
                    }
                    g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_IDLE;
                    g_map_resource_load_state = MAP_LOAD_STATE_DONE;
                    map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
                    main_heap_free(g_map_data_load_buffer);
                }
            }
        }
    } else {
        switch (g_map_resource_load_state) {
        case MAP_LOAD_STATE_GNS_FILE:
            if (main_file_is_still_loading() == 0) {
                gns_record_t* source;
                gns_record_t* destination;

                i = 0;
                source = g_map_data_load_buffer->gns_records;
                destination = g_map_gns_records;
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
                do {
                    *destination = *source;
                    destination++;
                    source++;
                    i++;
                } while (i < 150);
            }
            break;
        case MAP_LOAD_STATE_RESOURCE_FILE:
            if (main_file_is_still_loading() == 0) {
                if (g_map_gns_dispatch_held != 0) {
                    return MAP_LOAD_RESULT_HELD;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
                map_dispatch_gns_resource(
                    g_map_pending_gns_records[g_map_gns_record_cursor].resource_type, g_map_data_load_buffer);
                g_map_loaded_gns_records[g_map_gns_record_cursor] = g_map_pending_gns_records[g_map_gns_record_cursor];
                g_map_gns_record_cursor++;
            }
            break;
        case MAP_LOAD_STATE_APPLY_LIGHTING:
            if (main_file_is_still_loading() == 0) {
                if (g_map_lighting_pending != 0) {
                    map_dispatch_gns_resource(
                        MAP_RESOURCE_APPLY_LIGHTING, (map_resource_t*)g_map_pending_lighting_data);
                }
                g_map_lighting_pending = 0;
                g_map_resource_load_state = MAP_LOAD_STATE_APPLY_TEXTURE;
            }
            break;
        case MAP_LOAD_STATE_APPLY_TEXTURE:
            if (g_map_texture_pending != 0) {
                map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE, g_map_data_load_buffer);
            }
            if (g_map_palette_pending != 0) {
                map_dispatch_gns_resource(MAP_RESOURCE_APPLY_PALETTES, (map_resource_t*)g_map_pending_palette_data);
            }
            g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
            break;
        /* Rebuild the mesh render records once in each render buffer: each
         * state waits until the current buffer is not the one last written. */
        case MAP_LOAD_STATE_MESH_RECORDS:
            if (g_map_mesh_last_render_buffer != g_battle_gfx_current_render_buffer) {
                g_map_textured_triangle_render_record_count = 0;
                g_map_textured_quad_render_record_count = 0;
                g_map_untextured_triangle_render_record_count = 0;
                g_map_untextured_quad_render_record_count = 0;
                for (i = 0; i < 9; i++) {
                    if (g_map_mesh_part_geometry[i] != 0) {
                        map_init_mesh_render_records(g_map_mesh_part_geometry[i]);
                    }
                    g_map_textured_triangle_render_record_count += g_map_mesh_parts[i].textured_triangle_count;
                    g_map_textured_quad_render_record_count += g_map_mesh_parts[i].textured_quad_count;
                    g_map_untextured_triangle_render_record_count += g_map_mesh_parts[i].untextured_triangle_count;
                    g_map_untextured_quad_render_record_count += g_map_mesh_parts[i].untextured_quad_count;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_MESH_RECORDS_SECOND;
            }
            break;
        case MAP_LOAD_STATE_MESH_RECORDS_SECOND:
            if (g_map_mesh_last_render_buffer != g_battle_gfx_current_render_buffer) {
                g_map_textured_triangle_render_record_count = 0;
                g_map_textured_quad_render_record_count = 0;
                g_map_untextured_triangle_render_record_count = 0;
                g_map_untextured_quad_render_record_count = 0;
                for (i = 0; i < 9; i++) {
                    if (g_map_mesh_part_geometry[i] != 0) {
                        map_init_mesh_render_records(g_map_mesh_part_geometry[i]);
                    }
                    g_map_mesh_part_geometry[i] = 0;
                    g_map_textured_triangle_render_record_count += g_map_mesh_parts[i].textured_triangle_count;
                    g_map_textured_quad_render_record_count += g_map_mesh_parts[i].textured_quad_count;
                    g_map_untextured_triangle_render_record_count += g_map_mesh_parts[i].untextured_triangle_count;
                    g_map_untextured_quad_render_record_count += g_map_mesh_parts[i].untextured_quad_count;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
            }
            break;
        }
    }
    return g_map_gns_load_phase;
}

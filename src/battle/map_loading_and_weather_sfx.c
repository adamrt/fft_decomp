/* Map loading and weather sfx. Its start is a cut chosen by topic; its end is a boundary the binary proves. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
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
s32 battle_map_load_data(s32 map_id, s32 command) {
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
            battle_map_load_gns_and_move_find_items(map_id & 0xffff);
            g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
            g_map_loaded_id = map_id;
            g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
            battle_map_reset_mesh_render_state();
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
                if (battle_map_load_gns_and_move_find_items(map_id & 0xffff) != g_map_gns_records) {
                    return 0;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
                g_map_loaded_id = map_id;
                g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
                battle_map_reset_mesh_render_state();
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
                        if (battle_map_is_gns_record_matching_state(
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
                        if (battle_map_is_gns_record_matching_state(
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
                        if (battle_map_is_gns_record_matching_state(
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
                    battle_map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[g_map_gns_record_cursor].load_info, g_map_data_load_buffer,
                        g_map_gns_record_cursor);
                    g_map_resource_load_state = MAP_LOAD_STATE_RESOURCE_FILE;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type != GNS_RECORD_NONE) {
                    main_system_handle_malloc_exception(2, 0x3cb);
                    g_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type = GNS_RECORD_NONE;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type = GNS_RECORD_NONE;
                    battle_map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].load_info,
                        g_map_data_load_buffer->bytes + 0x8000, MAP_RESOURCE_TEXTURE_QUARTER_0);
                    g_map_texture_quarter_0_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type = GNS_RECORD_NONE;
                    battle_map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].load_info,
                        g_map_data_load_buffer->bytes + 0x10000, MAP_RESOURCE_TEXTURE_QUARTER_1);
                    g_map_texture_quarter_1_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type = GNS_RECORD_NONE;
                    battle_map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].load_info,
                        g_map_data_load_buffer->bytes + 0x18000, MAP_RESOURCE_TEXTURE_QUARTER_2);
                    g_map_texture_quarter_2_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type = GNS_RECORD_NONE;
                    battle_map_start_file_load(g_map_directory_path,
                        &g_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].load_info,
                        g_map_data_load_buffer->bytes + 0x20000, MAP_RESOURCE_TEXTURE_QUARTER_3);
                    g_map_texture_quarter_3_pending = 1;
                    g_map_texture_pending = 1;
                } else if (g_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type != GNS_RECORD_NONE) {
                    g_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type = GNS_RECORD_NONE;
                    battle_map_start_file_load(g_map_directory_path,
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
                        battle_map_apply_gns_command(2, (map_resource_t*)g_map_gns_record_cursor);
                    }
                    g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_IDLE;
                    g_map_resource_load_state = MAP_LOAD_STATE_DONE;
                    battle_map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
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
                battle_map_dispatch_gns_resource(
                    g_map_pending_gns_records[g_map_gns_record_cursor].resource_type, g_map_data_load_buffer);
                g_map_loaded_gns_records[g_map_gns_record_cursor] = g_map_pending_gns_records[g_map_gns_record_cursor];
                g_map_gns_record_cursor++;
            }
            break;
        case MAP_LOAD_STATE_APPLY_LIGHTING:
            if (main_file_is_still_loading() == 0) {
                if (g_map_lighting_pending != 0) {
                    battle_map_dispatch_gns_resource(
                        MAP_RESOURCE_APPLY_LIGHTING, (map_resource_t*)g_map_pending_lighting_data);
                }
                g_map_lighting_pending = 0;
                g_map_resource_load_state = MAP_LOAD_STATE_APPLY_TEXTURE;
            }
            break;
        case MAP_LOAD_STATE_APPLY_TEXTURE:
            if (g_map_texture_pending != 0) {
                battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE, g_map_data_load_buffer);
            }
            if (g_map_palette_pending != 0) {
                battle_map_dispatch_gns_resource(
                    MAP_RESOURCE_APPLY_PALETTES, (map_resource_t*)g_map_pending_palette_data);
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
                        battle_map_init_mesh_render_records(g_map_mesh_part_geometry[i]);
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
                        battle_map_init_mesh_render_records(g_map_mesh_part_geometry[i]);
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
s32 battle_map_load_alternate_mesh(s32 palette_mode) {
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
                if (battle_map_is_gns_record_matching_state(
                        g_map_gns_records[i].map_state_comparison, g_map_gns_records[i].map_state, state)) {
                    g_map_selected_mesh_record = g_map_gns_records[i];
                }
            }
        }
        g_map_mesh_load_buffer = main_heap_alloc(0x20000);
        if (g_map_mesh_load_buffer == 0) {
            main_system_handle_malloc_exception(2, 0x3c9);
        }
        battle_map_start_file_load(g_map_directory_path, &g_map_selected_mesh_record.load_info, g_map_mesh_load_buffer,
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
            battle_map_update_palette_colors(palette_mode, MAP_CLUT_ROW_MARKED_TILES, 0, 1, mesh_data->clut);
            main_heap_free(g_map_mesh_load_buffer);
            g_map_mesh_load_state = MAP_MESH_LOAD_IDLE;
        }
        break;
    }
    return g_map_mesh_load_state;
}

/*
 * The geometry block starts with four polygon counts, then stores positions
 * for all four polygon classes, normals for textured polygons, per-polygon
 * render data, and finally terrain-tile locations for textured polygons.
 * Each position/normal vector occupies eight renderer bytes: XYZ followed by
 * a spare 16-bit value.  The first position vector's spare 16-bit value holds the
 * polygon's packed terrain-tile location.
 * Target 0x800f4dd4 is the geometry pass for primary and animated meshes;
 * map_init_mesh_render_records performs the separate packet
 * pass. Count order is textured triangles/quads, then untextured
 * triangles/quads. The mesh metadata keeps the starting indices and the
 * incoming counts; the persistent totals advance at the end.
 */

void battle_map_append_mesh_geometry(const map_mesh_geometry_t* geometry_data, map_mesh_part_t* metadata) {
    /* Named count loads fold the retail four 16-bit cursor advances. */
    const u16* source = &geometry_data->textured_triangle_count;
    s32 i;
    s32 textured_triangle_count;
    s32 textured_quad_count;
    s32 untextured_triangle_count;
    s32 untextured_quad_count;

    g_map_resource_load_state = MAP_LOAD_STATE_MESH_RECORDS;
    g_map_mesh_last_render_buffer = g_battle_gfx_current_render_buffer;

    textured_triangle_count = *source++;
    textured_quad_count = *source++;
    untextured_triangle_count = *source++;
    untextured_quad_count = *source++;

    metadata->textured_triangle_start = g_map_textured_triangle_count;
    metadata->textured_quad_start = g_map_textured_quad_count;
    metadata->untextured_triangle_start = g_map_untextured_triangle_count;
    metadata->untextured_quad_start = g_map_untextured_quad_count;
    metadata->textured_triangle_count = textured_triangle_count;
    metadata->textured_quad_count = textured_quad_count;
    metadata->untextured_triangle_count = untextured_triangle_count;
    metadata->untextured_quad_count = untextured_quad_count;

    for (i = g_map_textured_triangle_count; i < g_map_textured_triangle_count + textured_triangle_count; i++) {
        g_map_textured_triangle_positions[i].x0 = *source++;
        g_map_textured_triangle_positions[i].y0 = *source++;
        g_map_textured_triangle_positions[i].z0 = *source++;
        g_map_textured_triangle_positions[i].x1 = *source++;
        g_map_textured_triangle_positions[i].y1 = *source++;
        g_map_textured_triangle_positions[i].z1 = *source++;
        g_map_textured_triangle_positions[i].x2 = *source++;
        g_map_textured_triangle_positions[i].y2 = *source++;
        g_map_textured_triangle_positions[i].z2 = *source++;
    }

    for (i = g_map_textured_quad_count; i < g_map_textured_quad_count + textured_quad_count; i++) {
        g_map_textured_quad_positions[i].x0 = *source++;
        g_map_textured_quad_positions[i].y0 = *source++;
        g_map_textured_quad_positions[i].z0 = *source++;
        g_map_textured_quad_positions[i].x1 = *source++;
        g_map_textured_quad_positions[i].y1 = *source++;
        g_map_textured_quad_positions[i].z1 = *source++;
        g_map_textured_quad_positions[i].x2 = *source++;
        g_map_textured_quad_positions[i].y2 = *source++;
        g_map_textured_quad_positions[i].z2 = *source++;
        g_map_textured_quad_positions[i].x3 = *source++;
        g_map_textured_quad_positions[i].y3 = *source++;
        g_map_textured_quad_positions[i].z3 = *source++;
    }

    for (i = g_map_untextured_triangle_count; i < g_map_untextured_triangle_count + untextured_triangle_count; i++) {
        g_map_untextured_triangle_positions[i].x0 = *source++;
        g_map_untextured_triangle_positions[i].y0 = *source++;
        g_map_untextured_triangle_positions[i].z0 = *source++;
        g_map_untextured_triangle_positions[i].x1 = *source++;
        g_map_untextured_triangle_positions[i].y1 = *source++;
        g_map_untextured_triangle_positions[i].z1 = *source++;
        g_map_untextured_triangle_positions[i].x2 = *source++;
        g_map_untextured_triangle_positions[i].y2 = *source++;
        g_map_untextured_triangle_positions[i].z2 = *source++;
    }

    for (i = g_map_untextured_quad_count; i < g_map_untextured_quad_count + untextured_quad_count; i++) {
        g_map_untextured_quad_positions[i].x0 = *source++;
        g_map_untextured_quad_positions[i].y0 = *source++;
        g_map_untextured_quad_positions[i].z0 = *source++;
        g_map_untextured_quad_positions[i].x1 = *source++;
        g_map_untextured_quad_positions[i].y1 = *source++;
        g_map_untextured_quad_positions[i].z1 = *source++;
        g_map_untextured_quad_positions[i].x2 = *source++;
        g_map_untextured_quad_positions[i].y2 = *source++;
        g_map_untextured_quad_positions[i].z2 = *source++;
        g_map_untextured_quad_positions[i].x3 = *source++;
        g_map_untextured_quad_positions[i].y3 = *source++;
        g_map_untextured_quad_positions[i].z3 = *source++;
    }

    for (i = g_map_textured_triangle_count; i < g_map_textured_triangle_count + textured_triangle_count; i++) {
        g_map_textured_triangle_normals[i].vertex[0].vx = *source++;
        g_map_textured_triangle_normals[i].vertex[0].vy = *source++;
        g_map_textured_triangle_normals[i].vertex[0].vz = *source++;
        g_map_textured_triangle_normals[i].vertex[1].vx = *source++;
        g_map_textured_triangle_normals[i].vertex[1].vy = *source++;
        g_map_textured_triangle_normals[i].vertex[1].vz = *source++;
        g_map_textured_triangle_normals[i].vertex[2].vx = *source++;
        g_map_textured_triangle_normals[i].vertex[2].vy = *source++;
        g_map_textured_triangle_normals[i].vertex[2].vz = *source++;
    }

    for (i = g_map_textured_quad_count; i < g_map_textured_quad_count + textured_quad_count; i++) {
        g_map_textured_quad_normals[i].vertex[0].vx = *source++;
        g_map_textured_quad_normals[i].vertex[0].vy = *source++;
        g_map_textured_quad_normals[i].vertex[0].vz = *source++;
        g_map_textured_quad_normals[i].vertex[1].vx = *source++;
        g_map_textured_quad_normals[i].vertex[1].vy = *source++;
        g_map_textured_quad_normals[i].vertex[1].vz = *source++;
        g_map_textured_quad_normals[i].vertex[2].vx = *source++;
        g_map_textured_quad_normals[i].vertex[2].vy = *source++;
        g_map_textured_quad_normals[i].vertex[2].vz = *source++;
        g_map_textured_quad_normals[i].vertex[3].vx = *source++;
        g_map_textured_quad_normals[i].vertex[3].vy = *source++;
        g_map_textured_quad_normals[i].vertex[3].vz = *source++;
    }

    /* Skip texture records (10/12 bytes) and untextured records (4 bytes). */
    source += textured_triangle_count * 5 + textured_quad_count * 6 + untextured_triangle_count * 2
        + untextured_quad_count * 2;

    for (i = g_map_textured_triangle_count; i < g_map_textured_triangle_count + textured_triangle_count; i++) {
        g_map_textured_triangle_positions[i].terrain_tile = *source++;
    }

    for (i = g_map_textured_quad_count; i < g_map_textured_quad_count + textured_quad_count; i++) {
        g_map_textured_quad_positions[i].terrain_tile = *source++;
    }

    g_map_textured_triangle_count += textured_triangle_count;
    g_map_textured_quad_count += textured_quad_count;
    g_map_untextured_triangle_count += untextured_triangle_count;
    g_map_untextured_quad_count += untextured_quad_count;
}

/*
 * Runtime polygon records interleave GPU packet fields with renderer state.
 * This pass skips the geometry and normal vectors consumed by
 * map_append_mesh_geometry, then appends UV coordinates, CLUT commands,
 * texture-page commands, and the initial state of untextured records.
 * Target 0x800f5578 reads textured triangles of five 16-bit values and textured quads
 * of six 16-bit values. 16-bit CLUT values keep their low six bits (the CLUT) and gain the map CLUT row;
 * texture pages are copied verbatim. Destination strides are 0x28/0x34 for
 * textured triangles/quads and 0x14/0x18 for untextured triangles/quads.
 * Untextured POLY_F3/POLY_F4 packets start with color 0/0/1.
 */
void battle_map_init_mesh_render_records(const map_mesh_geometry_t* geometry_data) {
    /* Named count loads fold the retail four 16-bit cursor advances. */
    const u16* source = &geometry_data->textured_triangle_count;
    s32 polygon_index;
    s32 textured_triangle_count;
    s32 textured_quad_count;
    s32 untextured_triangle_count;
    s32 untextured_quad_count;
    s32 triangle_vector_halfwords;
    s32 quad_vector_halfwords;
    u16 packed;
    s32 u0, v0, u1, v1, u2, v2, u3, v3;
    u16 clut_data;
    u16 texture_page;

    g_map_mesh_last_render_buffer = g_battle_gfx_current_render_buffer;

    textured_triangle_count = *source++;
    textured_quad_count = *source++;
    untextured_triangle_count = *source++;
    untextured_quad_count = *source++;

    triangle_vector_halfwords = textured_triangle_count * 9;
    quad_vector_halfwords = textured_quad_count * 12;
    source += triangle_vector_halfwords + quad_vector_halfwords + untextured_triangle_count * 9
        + untextured_quad_count * 12 + triangle_vector_halfwords + quad_vector_halfwords;

    for (polygon_index = g_map_textured_triangle_render_record_count;
        polygon_index < textured_triangle_count + g_map_textured_triangle_render_record_count; polygon_index++) {
        packed = *source++;
        v0 = packed >> 8;
        u0 = packed & 0xff;
        packed = *source++;
        clut_data = packed;
        packed = *source++;
        v1 = packed >> 8;
        u1 = packed & 0xff;
        packed = *source++;
        texture_page = packed;
        packed = *source++;
        v2 = packed >> 8;
        u2 = packed & 0xff;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].u0 = u0;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].u1 = u1;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].u2 = u2;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].v0 = v0;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].v1 = v1;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].v2 = v2;
        g_battle_gfx_current_render_buffer->gt3[polygon_index].clut
            = (clut_data & 0x3f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        g_battle_gfx_current_render_buffer->gt3[polygon_index].tpage = texture_page;
    }

    for (polygon_index = g_map_textured_quad_render_record_count;
        polygon_index < textured_quad_count + g_map_textured_quad_render_record_count; polygon_index++) {
        packed = *source++;
        v0 = packed >> 8;
        u0 = packed & 0xff;
        packed = *source++;
        clut_data = packed;
        packed = *source++;
        v1 = packed >> 8;
        u1 = packed & 0xff;
        packed = *source++;
        texture_page = packed;
        packed = *source++;
        v2 = packed >> 8;
        u2 = packed & 0xff;
        packed = *source++;
        v3 = packed >> 8;
        u3 = packed & 0xff;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].u0 = u0;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].u1 = u1;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].u2 = u2;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].u3 = u3;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].v0 = v0;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].v1 = v1;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].v2 = v2;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].v3 = v3;
        g_battle_gfx_current_render_buffer->gt4[polygon_index].clut
            = (clut_data & 0x3f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        g_battle_gfx_current_render_buffer->gt4[polygon_index].tpage = texture_page;
    }

    for (polygon_index = g_map_untextured_triangle_render_record_count;
        polygon_index < untextured_triangle_count + g_map_untextured_triangle_render_record_count; polygon_index++) {
        g_battle_gfx_current_render_buffer->f3[polygon_index].r0 = 0;
        g_battle_gfx_current_render_buffer->f3[polygon_index].g0 = 0;
        g_battle_gfx_current_render_buffer->f3[polygon_index].b0 = 1;
    }

    for (polygon_index = g_map_untextured_quad_render_record_count;
        polygon_index < untextured_quad_count + g_map_untextured_quad_render_record_count; polygon_index++) {
        g_battle_gfx_current_render_buffer->f4[polygon_index].r0 = 0;
        g_battle_gfx_current_render_buffer->f4[polygon_index].g0 = 0;
        g_battle_gfx_current_render_buffer->f4[polygon_index].b0 = 1;
    }
}

/* Stop the weather sound effects and every color rotation the 32 map texture
 * animations hold a handle for. Always returns 0. */
s32 battle_map_stop_clut_cycles_and_weather_sfx(void) {
    s32 index;

    battle_sound_stop_weather_sfx();

    for (index = 0; index < MAP_TEXTURE_ANIMATION_COUNT; index++) {
        if (g_map_texture_animation_cycle_handles[index] != 0) {
            battle_map_stop_clut_cycle(g_map_texture_animation_cycle_handles[index]);
            g_map_texture_animation_cycle_handles[index] = 0;
        }
    }

    g_sound_weather_sfx_enabled = 0;
    return 0;
}

/* Enable the weather sounds and start the rain sound when the weather has set one
 * (g_sound_primary_weather_sfx_id). */
void battle_sound_start_weather_sfx(void) {
    s32 sound_id;

    sound_id = g_sound_primary_weather_sfx_id;
    g_sound_weather_sfx_enabled = 1;
    if (sound_id != 0) {
        main_sound_play_bg_sfx(sound_id);
    }
}

/* Start map weather effect mode 0x96 for `value` frames, from frame 0; effect
 * E458 passes the frames left of its timeline. An id other than 0x96 does
 * nothing. D_800F668C receives 0x95 and no code reads it. */
void battle_map_set_command_0x96_duration(s32 id, s32 value) {
    if (id == MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
        g_battle_unknown_800f668c = 0x95;
        g_map_command_0x96_duration = value;
        g_map_command_0x96_frame = 0;
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM;
    }
}

/* Stop the rain sound and the two thunder sounds and forget their ids. The result
 * is an unset local; no caller reads it. */
s32 battle_sound_stop_weather_sfx(void) {
    s32 result;
    s32 sound_id;

    sound_id = g_sound_primary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_primary_weather_sfx_id = 0;
    }

    sound_id = g_sound_secondary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_secondary_weather_sfx_id = 0;
    }

    sound_id = g_sound_tertiary_weather_sfx_id;
    if (sound_id != 0) {
        main_sound_stop_sfx(sound_id);
        g_sound_tertiary_weather_sfx_id = 0;
    }

    /* The target returns an indeterminate saved-register value. */
    return result;
}

#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* g_battle_map_gns_records viewed with the loader's halfword-aligned type,
 * bound separately: a cast of the canonical array copies each record through
 * a stack temporary. */
extern gns_record_view_t g_battle_map_gns_records_view[150];

s32 battle_map_load_data(s32 map_id, s32 command) {
    s32 i;
    s32 r;
    s32 j;
    s16 state;
    u8 diff;

    if (g_battle_map_gns_load_phase == 0) {
        switch (command) {
        case 0x73:
            if (main_file_is_still_loading() != 0) {
                return 0;
            }
            g_battle_map_data_load_buffer = game_malloc(0x20000);
            if (g_battle_map_data_load_buffer == 0) {
                main_system_handle_malloc_exception(2, 0x3c2);
            }
            battle_map_load_gns_and_move_find_items(map_id & 0xffff);
            g_battle_map_resource_load_state = 0x74;
            g_battle_map_loaded_id = map_id;
            g_battle_map_gns_load_phase = 0xf;
            battle_map_reset_mesh_render_state();
            break;
        case 0x99:
            g_battle_map_gns_dispatch_held = 1;
        case 0x75:
            if (main_file_is_still_loading() != 0) {
                return 0;
            }
            g_battle_map_data_load_buffer = game_malloc(0x20000);
            if (g_battle_map_data_load_buffer == 0) {
                main_system_handle_malloc_exception(2, 0x3c2);
            }
            g_battle_map_texture_quarter_0_pending = 0;
            g_battle_map_texture_quarter_1_pending = 0;
            g_battle_map_texture_quarter_2_pending = 0;
            g_battle_map_texture_quarter_3_pending = 0;
            g_battle_map_texture_pending = 0;
            g_battle_map_palette_pending = 0;
            g_battle_map_lighting_pending = 0;
            if (map_id != g_battle_map_loaded_id) {
                if (battle_map_load_gns_and_move_find_items(map_id & 0xffff) != g_battle_map_gns_records) {
                    return 0;
                }
                g_battle_map_resource_load_state = 0x74;
                g_battle_map_loaded_id = map_id;
                g_battle_map_gns_load_phase = 0xf;
                battle_map_reset_mesh_render_state();
            } else {
                g_battle_map_resource_load_state = 0;
                g_battle_map_gns_load_phase = 0xf;
            }
            break;
        case 0x74:
        case 0x9c:
            break;
        }
    }

    if (g_battle_map_resource_load_state == 0) {
        if (g_battle_map_gns_load_phase != 1) {
            if (g_battle_map_gns_load_phase == 0xf) {
                g_battle_map_gns_record_cursor = 0;
                g_battle_map_gns_load_phase = 1;
                {
                    s32 k;

                    for (k = 0; k < 49; k++) {
                        g_battle_map_pending_gns_records[k].resource_type = 0;
                    }
                }
                for (r = 0; g_battle_map_gns_records_view[r].resource_type < 0x80; r++) {
                    if (g_battle_map_gns_records_view[r].resource_type == GNS_RESOURCE_MESH_PRIMARY) {
                        state = battle_script_get_variable(g_battle_map_gns_records_view[r].script_variable_id) & 0xfff;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if ((s16)battle_map_is_gns_record_matching_state(
                                g_battle_map_gns_records_view[r].map_state_comparison,
                                g_battle_map_gns_records_view[r].map_state, state)) {
                            /* No braces: a braced body's block note lengthens this loop by one
                             * luid, and loop.c then hoists the 0x48 out of the record loop. */
                            j = 0;
                            do
                                g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                    .load_info[j] = 0x48;
                            while (++j < 14);
                        }
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                            .load_info[k]
                                    ^ g_battle_map_gns_records_view[r].load_info[k];
                            }
                        }
                        if (diff) {
                            g_battle_map_pending_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                = g_battle_map_gns_records_view[r];
                        }
                    }
                }
                for (r = 0; g_battle_map_gns_records_view[r].resource_type < 0x80; r++) {
                    if (g_battle_map_gns_records_view[r].resource_type == GNS_RESOURCE_MESH_OVERRIDE) {
                        state = battle_script_get_variable(g_battle_map_gns_records_view[r].script_variable_id) & 0xfff;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if ((s16)battle_map_is_gns_record_matching_state(
                                g_battle_map_gns_records_view[r].map_state_comparison,
                                g_battle_map_gns_records_view[r].map_state, state)) {
                            j = 0;
                            do
                                g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                    .load_info[j] = 0x48;
                            while (++j < 14);
                        }
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                            .load_info[k]
                                    ^ g_battle_map_gns_records_view[r].load_info[k];
                            }
                        }
                        if (diff) {
                            g_battle_map_pending_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                = g_battle_map_gns_records_view[r];
                        }
                    }
                }
                for (r = 0; g_battle_map_gns_records_view[r].resource_type < 0x80; r++) {
                    if (g_battle_map_gns_records_view[r].resource_type == GNS_RESOURCE_TEXTURE
                        && (g_battle_map_gns_records_view[r].script_variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME
                            || g_battle_map_gns_records_view[r].script_variable_id == EVENT_SCRIPT_VAR_MAP_ARRANGEMENT)
                        && g_battle_map_gns_records_view[r].map_state == 0) {
                        {
                            s32 k;

                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                            .load_info[k]
                                    ^ g_battle_map_gns_records_view[r].load_info[k];
                            }
                        }
                        if (diff) {
                            g_battle_map_pending_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                = g_battle_map_gns_records_view[r];
                        }
                    }
                }
                for (r = 0; g_battle_map_gns_records_view[r].resource_type < 0x80; r++) {
                    if (g_battle_map_gns_records_view[r].resource_type != 0
                        && (g_battle_map_gns_records_view[r].script_variable_id != EVENT_SCRIPT_VAR_WEATHER_AND_TIME
                            || g_battle_map_gns_records_view[r].map_state != 0)
                        && (g_battle_map_gns_records_view[r].script_variable_id != EVENT_SCRIPT_VAR_MAP_ARRANGEMENT
                            || g_battle_map_gns_records_view[r].map_state != 0)) {
                        state = battle_script_get_variable(g_battle_map_gns_records_view[r].script_variable_id) & 0xfff;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if ((s16)battle_map_is_gns_record_matching_state(
                                g_battle_map_gns_records_view[r].map_state_comparison,
                                g_battle_map_gns_records_view[r].map_state, state)) {
                            {
                                s32 k;

                                diff = 0;
                                for (k = 0; k < 14; k++) {
                                    diff |= g_battle_map_loaded_gns_records[g_battle_map_gns_records_view[r]
                                                                                .resource_type]
                                                .load_info[k]
                                        ^ g_battle_map_gns_records_view[r].load_info[k];
                                }
                            }
                            if (diff) {
                                g_battle_map_pending_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                    = g_battle_map_gns_records_view[r];
                            } else {
                                g_battle_map_pending_gns_records[g_battle_map_gns_records_view[r].resource_type]
                                    .resource_type = 0;
                            }
                        }
                    }
                }
            }
        } else {
            if (main_file_is_still_loading() == 0) {
                while ((g_battle_map_pending_gns_records[g_battle_map_gns_record_cursor].resource_type == 0
                           && (u32)g_battle_map_gns_record_cursor < 49)
                    || (u32)(g_battle_map_pending_gns_records[g_battle_map_gns_record_cursor].resource_type - 0x12)
                        < 6) {
                    g_battle_map_gns_record_cursor++;
                }
                if ((u32)g_battle_map_gns_record_cursor < 49) {
                    if (g_battle_map_gns_record_cursor == 0x1b) {
                        bzero(g_battle_map_data_load_buffer, 0x280);
                    }
                    battle_map_start_file_load(g_battle_map_file_table,
                        g_battle_map_pending_gns_records[g_battle_map_gns_record_cursor].load_info,
                        (s32)g_battle_map_data_load_buffer, g_battle_map_gns_record_cursor);
                    g_battle_map_resource_load_state = 0x78;
                } else if (g_battle_map_pending_gns_records[18].resource_type != 0) {
                    main_system_handle_malloc_exception(2, 0x3cb);
                    g_battle_map_pending_gns_records[18].resource_type = 0;
                } else if (g_battle_map_pending_gns_records[19].resource_type != 0) {
                    g_battle_map_pending_gns_records[19].resource_type = 0;
                    battle_map_start_file_load(g_battle_map_file_table, g_battle_map_pending_gns_records[19].load_info,
                        (s32)g_battle_map_data_load_buffer + 0x8000, 0x13);
                    g_battle_map_texture_quarter_0_pending = 1;
                    g_battle_map_texture_pending = 1;
                } else if (g_battle_map_pending_gns_records[20].resource_type != 0) {
                    g_battle_map_pending_gns_records[20].resource_type = 0;
                    battle_map_start_file_load(g_battle_map_file_table, g_battle_map_pending_gns_records[20].load_info,
                        (s32)g_battle_map_data_load_buffer + 0x10000, 0x14);
                    g_battle_map_texture_quarter_1_pending = 1;
                    g_battle_map_texture_pending = 1;
                } else if (g_battle_map_pending_gns_records[21].resource_type != 0) {
                    g_battle_map_pending_gns_records[21].resource_type = 0;
                    battle_map_start_file_load(g_battle_map_file_table, g_battle_map_pending_gns_records[21].load_info,
                        (s32)g_battle_map_data_load_buffer + 0x18000, 0x15);
                    g_battle_map_texture_quarter_2_pending = 1;
                    g_battle_map_texture_pending = 1;
                } else if (g_battle_map_pending_gns_records[22].resource_type != 0) {
                    g_battle_map_pending_gns_records[22].resource_type = 0;
                    battle_map_start_file_load(g_battle_map_file_table, g_battle_map_pending_gns_records[22].load_info,
                        (s32)g_battle_map_data_load_buffer + 0x20000, 0x16);
                    g_battle_map_texture_quarter_3_pending = 1;
                    g_battle_map_texture_pending = 1;
                } else if (g_battle_map_pending_gns_records[23].resource_type != 0) {
                    g_battle_map_pending_gns_records[23].resource_type = 0;
                    battle_map_start_file_load(g_battle_map_file_table, g_battle_map_pending_gns_records[23].load_info,
                        (s32)g_battle_map_data_load_buffer, 0x17);
                    g_battle_map_texture_quarter_0_pending = 1;
                    g_battle_map_texture_quarter_1_pending = 1;
                    g_battle_map_texture_quarter_2_pending = 1;
                    g_battle_map_texture_quarter_3_pending = 1;
                    g_battle_map_texture_pending = 1;
                    g_battle_map_resource_load_state = 0x92;
                } else {
                    for (i = 0; g_battle_map_gns_records_view[i].resource_type < 0x80; i++) {
                        if (g_battle_map_gns_records_view[i].resource_type == 0) {
                            break;
                        }
                    }
                    i++;
                    g_battle_map_gns_record_cursor = (s32)&g_battle_map_gns_records[i];
                    while (((gns_command_record_prefix_t*)g_battle_map_gns_record_cursor)->resource_type != 0) {
                        battle_process_map_gns_record(2, (gns_command_record_prefix_t*)g_battle_map_gns_record_cursor);
                    }
                    g_battle_map_gns_load_phase = 0;
                    g_battle_map_resource_load_state = 0x77;
                    battle_map_set_weather_texture_overlay(0x8b);
                    main_heap_free(g_battle_map_data_load_buffer);
                }
            }
        }
    } else {
        switch (g_battle_map_resource_load_state) {
        case 0x74:
            if (main_file_is_still_loading() == 0) {
                gns_record_view_t* source;
                gns_record_view_t* destination;

                i = 0;
                source = g_battle_map_data_load_buffer;
                destination = g_battle_map_gns_records_view;
                g_battle_map_resource_load_state = 0;
                do {
                    *destination = *source;
                    destination++;
                    source++;
                    i++;
                } while (i < 150);
            }
            break;
        case 0x78:
            if (main_file_is_still_loading() == 0) {
                if (g_battle_map_gns_dispatch_held != 0) {
                    return 0x9b;
                }
                g_battle_map_resource_load_state = 0;
                battle_map_dispatch_gns_resource(
                    g_battle_map_pending_gns_records[g_battle_map_gns_record_cursor].resource_type,
                    (u8*)g_battle_map_data_load_buffer);
                g_battle_map_loaded_gns_records[g_battle_map_gns_record_cursor]
                    = g_battle_map_pending_gns_records[g_battle_map_gns_record_cursor];
                g_battle_map_gns_record_cursor++;
            }
            break;
        case 0x92:
            if (main_file_is_still_loading() == 0) {
                if (g_battle_map_lighting_pending != 0) {
                    battle_map_dispatch_gns_resource(0x32, g_battle_map_pending_lighting_data);
                }
                g_battle_map_lighting_pending = 0;
                g_battle_map_resource_load_state = 0x93;
            }
            break;
        case 0x93:
            if (g_battle_map_texture_pending != 0) {
                battle_map_dispatch_gns_resource(0x17, (u8*)g_battle_map_data_load_buffer);
            }
            if (g_battle_map_palette_pending != 0) {
                battle_map_dispatch_gns_resource(0x33, g_battle_map_pending_palette_data);
            }
            g_battle_map_resource_load_state = 0;
            break;
        case 0x79:
            if (g_battle_map_mesh_data_buffer != (s32)g_battle_data) {
                g_battle_map_textured_triangle_render_record_count = 0;
                g_battle_map_textured_quad_render_record_count = 0;
                g_battle_map_untextured_triangle_render_record_count = 0;
                g_battle_map_untextured_quad_render_record_count = 0;
                for (i = 0; i < 9; i++) {
                    if (g_battle_map_mesh_part_geometry[i] != 0) {
                        battle_map_init_mesh_render_records((u16*)g_battle_map_mesh_part_geometry[i]);
                    }
                    g_battle_map_textured_triangle_render_record_count += g_battle_map_mesh_parts[i].counts[0];
                    g_battle_map_textured_quad_render_record_count += g_battle_map_mesh_parts[i].counts[1];
                    g_battle_map_untextured_triangle_render_record_count += g_battle_map_mesh_parts[i].counts[2];
                    g_battle_map_untextured_quad_render_record_count += g_battle_map_mesh_parts[i].counts[3];
                }
                g_battle_map_resource_load_state = 0x91;
            }
            break;
        case 0x91:
            if (g_battle_map_mesh_data_buffer != (s32)g_battle_data) {
                g_battle_map_textured_triangle_render_record_count = 0;
                g_battle_map_textured_quad_render_record_count = 0;
                g_battle_map_untextured_triangle_render_record_count = 0;
                g_battle_map_untextured_quad_render_record_count = 0;
                for (i = 0; i < 9; i++) {
                    if (g_battle_map_mesh_part_geometry[i] != 0) {
                        battle_map_init_mesh_render_records((u16*)g_battle_map_mesh_part_geometry[i]);
                    }
                    g_battle_map_mesh_part_geometry[i] = 0;
                    g_battle_map_textured_triangle_render_record_count += g_battle_map_mesh_parts[i].counts[0];
                    g_battle_map_textured_quad_render_record_count += g_battle_map_mesh_parts[i].counts[1];
                    g_battle_map_untextured_triangle_render_record_count += g_battle_map_mesh_parts[i].counts[2];
                    g_battle_map_untextured_quad_render_record_count += g_battle_map_mesh_parts[i].counts[3];
                }
                g_battle_map_resource_load_state = 0;
            }
            break;
        }
    }
    return g_battle_map_gns_load_phase;
}

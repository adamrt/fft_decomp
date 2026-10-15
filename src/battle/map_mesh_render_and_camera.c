/* Map mesh render and camera. Its start is an object boundary the read-only data layout places somewhere nearby; its
 * end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/*
 * Reset the battle map's mesh, draw-mode and per-part render state before a
 * map load.
 *
 * Clears the four polygon counts and position spare 16-bit values, the nine mesh
 * part records (vector triples reset to zero/unit scale) and several map
 * tables. The weather flags keep only bits 0x7d.
 */
void battle_map_reset_mesh_render_state(void) {
    SVECTOR zero_vector;
    VECTOR unit_scale;
    s32 i;
    s16 no_tile;
    s32 j;
    s32* weather_flags;

    zero_vector = g_map_zero_svector;
    unit_scale = g_map_unit_scale_vector;
    g_map_tile_width = 1;
    g_map_tile_depth = 1;
    g_target_color_tile_x = 1;
    g_target_color_tile_y = 1;
    g_map_untextured_otz_mask = 0;
    g_map_untextured_fixed_ot_index = 4;
    g_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            g_battle_unknown_800f70b4[i][j] = 0;
        }
        g_battle_unknown_800f78e8[i] = 0;
    }
    for (i = 0; i < GNS_RESOURCE_TABLE_COUNT; i++) {
        bzero(g_map_loaded_gns_records[i].load_info.bytes, sizeof(gns_load_info_t));
        bzero(&g_map_pending_gns_records[i].resource_type, 14);
    }
    battle_map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
    g_map_lightning_mode = MAP_LIGHTNING_OFF;
    weather_flags = &g_map_weather_flags;
    *weather_flags &= 0x7d;
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    g_map_ambient_polygon_color.r = 0x80;
    g_map_ambient_polygon_color.g = 0x80;
    g_map_ambient_polygon_color.b = 0x80;
    for (i = 0; i < 360; i++) {
        g_map_textured_triangle_positions[i].polygon_flags = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 710; i++) {
        g_map_textured_quad_positions[i].polygon_flags = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 64; i++) {
        g_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < 256; i++) {
        g_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        g_map_texture_animations[i].active = 0;
    }
    /* A signed sentinel local retains each array's original -2 constant load. */
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 360; i++) {
        g_map_textured_triangle_positions[i].terrain_tile = no_tile;
    }
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 710; i++) {
        g_map_textured_quad_positions[i].terrain_tile = no_tile;
    }
    for (i = 1; i < 9; i++) {
        g_map_mesh_animation_instructions[i].states[0][1].next = 0;
        g_map_mesh_parts[i].speed = 0x10;
        g_map_mesh_parts[i].keyframe = 0;
        g_map_mesh_part_animation_states[i] = 0;
    }
    g_map_textured_triangle_count = 0;
    g_map_textured_quad_count = 0;
    g_map_untextured_triangle_count = 0;
    g_map_untextured_quad_count = 0;
    for (i = 0; i < 9; i++) {
        g_map_mesh_parts[i].textured_triangle_count = 0;
        g_map_mesh_parts[i].textured_quad_count = 0;
        g_map_mesh_parts[i].untextured_triangle_count = 0;
        g_map_mesh_parts[i].untextured_quad_count = 0;
        g_map_mesh_parts[i].rotation = zero_vector;
        g_map_mesh_parts[i].rotation_start = zero_vector;
        g_map_mesh_parts[i].rotation_step = zero_vector;
        g_map_mesh_parts[i].translation = zero_vector;
        g_map_mesh_parts[i].translation_start = zero_vector;
        g_map_mesh_parts[i].translation_step = zero_vector;
        g_map_mesh_parts[i].scale = unit_scale;
        g_map_mesh_parts[i].scale_start = unit_scale;
        g_map_mesh_parts[i].scale_step = unit_scale;
        g_map_mesh_parts[i].next_instruction = 1;
        g_map_mesh_part_animation_states[i] = 0;
    }
}

/* Rebuilds the camera matrix from the angles (`rotation`, an SVECTOR), the
 * screen offset and the zoom.
 *
 * The matrix is the rotation scaled by the zoom, with a translation that puts
 * g_camera_integer_coords (the negated camera position) at the screen
 * offset. It is stored in *camera_matrix, g_map_camera_matrix and map mesh
 * part 0; the angles and zoom are kept in g_map_camera_rotation and
 * g_map_camera_zoom, and the light rotation matrix is rebuilt. The GTE is
 * left with the scaled rotation and the bare screen offset as its translation. */
void battle_camera_update_matrices(
    MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom) {
    long result;

    g_map_camera_zoom = *camera_zoom;
    memcpy(&g_map_camera_rotation, rotation, sizeof(g_map_camera_rotation));

    RotMatrix(rotation, camera_matrix);
    TransMatrix(camera_matrix, screen_offset);
    ScaleMatrix(camera_matrix, &g_map_camera_zoom);
    RotMatrix(rotation, &g_map_camera_matrix);
    SetRotMatrix(camera_matrix);
    SetTransMatrix(camera_matrix);
    RotTrans(&g_camera_integer_coords, (VECTOR*)g_map_camera_matrix.t, &result);
    ScaleMatrix(&g_map_camera_matrix, &g_map_camera_zoom);

    *camera_matrix = g_map_camera_matrix;
    g_map_mesh_parts[0].matrix = g_map_camera_matrix;
    RotMatrix(&g_map_light_rotation, &g_map_light_rotation_matrix);
}

/* Run a weather display command.
 *
 * Command 0x8b reads the weather from script variable 0x23 and re-issues the
 * set-up commands for values 0-1, 2, 3 and 4, choosing each variant by the
 * snow bit of the map weather flags. The weather local is an s8: compares on
 * the promoted value copy it into a scratch register for the 3 and 4 tests,
 * as the target does. */
void battle_map_run_weather_command(map_weather_command_e command) {
    s8 weather;

    switch (command & 0xff) {
    case MAP_WEATHER_COMMAND_RESTORE_LIGHTING:
        battle_map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
        SetColorMatrix(g_map_lightning_state.color_matrices);
        SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        break;
    case MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER:
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        g_map_lightning_mode = MAP_LIGHTNING_OFF;
        weather &= 7;
        if (weather < BATTLE_WEATHER_RAIN) {
            battle_sound_stop_weather_sfx();
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_RAIN) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_SNOW);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_RAIN);
            }
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STORM) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_SNOW);
            } else {
                g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_RAIN);
            }
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STRONG_STORM) {
            if (g_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_SNOW);
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_RAIN);
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHTNING);
                g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
            }
        }
        break;
    case MAP_WEATHER_COMMAND_THUNDER:
        g_map_lightning_mode = MAP_LIGHTNING_ONCE;
        g_map_lightning_state.timer = 0x3c;
        g_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
        if (g_sound_weather_sfx_enabled != 0) {
            main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
        }
        g_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_LIGHTNING:
        g_map_lightning_mode = MAP_LIGHTNING_RANDOM;
        g_map_lightning_state.timer = 0x3c;
        g_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_SNOW:
        g_map_weather_fall_speed = 1;
        g_map_weather_primary_speed = 4;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_SNOW:
        g_map_weather_fall_speed = 1;
        g_map_weather_primary_speed = 1;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_SNOW:
        g_map_weather_fall_speed = 3;
        g_map_weather_primary_speed = 0x10;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_RAIN:
        g_map_weather_fall_speed = 5;
        g_map_weather_primary_speed = 6;
        g_map_rain_secondary_fall_speed = 9;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_RAIN:
        g_map_weather_fall_speed = 3;
        g_map_weather_primary_speed = 5;
        g_map_rain_secondary_fall_speed = 8;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_RAIN:
        g_map_weather_fall_speed = 5;
        g_map_weather_primary_speed = 9;
        g_map_rain_secondary_fall_speed = 0x12;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_CLEAR:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_CLEAR;
        g_map_lightning_mode = MAP_LIGHTNING_OFF;
        break;
    }
}

/* Flat-textured triangle; the psx/libgpu.h clean-room set does not declare it. */

/* Set up the weather particles and their flat-textured sprites.
 *
 * 0x8f (snow, from map_run_weather_command 0x53/0x7e/0x7f) scatters 64 flakes;
 * 0x90 (rain, 0x55/0x7c/0x7d) scatters 32 drop pairs over the map extent and
 * records the lowest tile surface (largest y) in g_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
void battle_map_init_weather_particles(map_weather_command_e command) {
    RECT rects[3];
    s16 sprite;
    s16 tile;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    memcpy(rects, g_map_weather_drop_uv_rects, sizeof(rects));
    switch (command & 0xff) {
    case MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_SNOW;
        for (i = 0; i < 64; i++) {
            g_map_weather_particles[i].y = -rand() % 240 - 16;
            g_map_weather_particles[i].fall_speed = g_map_weather_fall_speed;
            sprite = rand() % 3;
            for (j = 0; j < 2; j++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 0);
                g_battle_render_buffers[j].ft3[i].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i].u0 = rects[sprite].x;
                g_battle_render_buffers[j].ft3[i].u1 = rects[sprite].x + rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].u2 = rects[sprite].x + rects[sprite].w;
                g_battle_render_buffers[j].ft3[i].v0 = rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v1 = rects[sprite].y;
                g_battle_render_buffers[j].ft3[i].v2 = rects[sprite].y + rects[sprite].h;
            }
            g_map_weather_particles[i].fall_speed = sprite + g_map_weather_fall_speed;
            if (rand() & 1) {
                g_map_weather_particles[i].x = rand() % 140;
            } else {
                g_map_weather_particles[i].x = -(rand() % 140);
            }
            if (rand() & 1) {
                g_map_weather_particles[i].z = rand() % 140;
            } else {
                g_map_weather_particles[i].z = -(rand() % 140);
            }
        }
        break;
    case MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES:
        g_map_weather_effect_mode = MAP_WEATHER_EFFECT_RAIN;
        for (i = 0; i < 32; i++) {
            g_map_weather_particles[i].x = rand() % (g_map_tile_width * MAP_TILE_SIZE);
            g_map_weather_particles[i].z = rand() % (g_map_tile_depth * MAP_TILE_SIZE);
            g_map_weather_particles[i].y = -(rand() % 384) - 384;
            g_map_weather_particles[i + 32].x = g_map_weather_particles[i].x;
            g_map_weather_particles[i + 32].z = g_map_weather_particles[i].z;
            g_map_weather_particles[i + 32].y = g_map_weather_particles[i].y - rand() % 48;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < 32; i++) {
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i], 1);
                g_battle_render_buffers[j].ft3[i].r0 = 0x64;
                g_battle_render_buffers[j].ft3[i].g0 = 0x64;
                g_battle_render_buffers[j].ft3[i].b0 = 0x64;
                g_battle_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                prim = (POLY_FT3*)((u8*)g_battle_render_buffers[0].ft3
                    + (j * sizeof(battle_render_buffer_t) + i * sizeof(POLY_FT3)));
                setUV3(prim, 0xd8, 0xaf, 0xf7, 0xaf, 0xd8, 0xaf);
                SetSemiTrans(&g_battle_render_buffers[j].ft3[i + 32], 1);
                g_battle_render_buffers[j].ft3[i + 32].r0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].g0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].b0 = 0x80;
                g_battle_render_buffers[j].ft3[i + 32].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_battle_render_buffers[j].ft3[i + 32].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_battle_render_buffers[j].ft3[i + 32].u0 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].u1 = 0x1f;
                g_battle_render_buffers[j].ft3[i + 32].v0 = 0xa0;
                g_battle_render_buffers[j].ft3[i + 32].u2 = 0x10;
                g_battle_render_buffers[j].ft3[i + 32].v1 = 0xaf;
                g_battle_render_buffers[j].ft3[i + 32].v2 = 0xaf;
            }
        }
        g_map_lowest_surface_y = 0x80000000;
        g_map_tile_data_ptr = g_map_tile_data;
        for (i = 0; i < g_map_tile_width; i++) {
            for (j = 0; j < g_map_tile_depth; j++) {
                tile = i + j * g_map_tile_width;
                surface_y
                    = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
                if (g_map_lowest_surface_y < surface_y) {
                    g_map_lowest_surface_y = surface_y;
                }
            }
        }
        break;
    }
}

/* Switch the ordering-table placement of untextured map polygons around an
 * ability effect: BEGIN places them by depth (OT z mask -1, fixed index 0), END
 * returns them to fixed slot 4 (mask 0), the values a map load sets. Returns 4
 * for END, and -1 for BEGIN or any other command. */
s32 battle_map_configure_effect_data_state(map_effect_data_command_e command) {
    s32 result;

    switch (command) {
    case MAP_EFFECT_DATA_END:
        result = 4;
        g_map_untextured_otz_mask = 0;
        g_map_untextured_fixed_ot_index = result;
        break;
    case MAP_EFFECT_DATA_BEGIN:
        result = -1;
        g_map_untextured_otz_mask = result;
        g_map_untextured_fixed_ot_index = 0;
        break;
    default:
        result = -1;
        break;
    }
    return result;
}

/* Advance the keyframe animations of map mesh parts 1-8 by one frame, from
 * map_draw_mesh_and_weather.
 *
 * A part whose running instruction row has a successor (states[0][1].next) adds
 * (speed >> 4) * g_main_system_animation_speed to its elapsed time. When that
 * passes the keyframe's duration the part asks for its next instruction; an
 * instruction of duration 0 ends the animation. For a new keyframe the current
 * rotation, translation and scale become the start values and each of the nine
 * channels takes its delta: target minus start for TWEEN_TO, the keyframe value
 * for TWEEN_BY. Each frame a channel is then start + delta * f, where f is
 * elapsed / duration (LINEAR), the sine of the angle interpolated from
 * angle_start to angle_end (SINE), or that sine plus 1.0 (RAISED_SINE).
 *
 * `next` is assigned only when a keyframe expires in the same pass. A part with a
 * nonzero animation state and keyframe 0 that did not expire indexes the
 * instructions with the value an earlier part left, or with an unset local.
 *
 * A channel with none of the three curve bits is not recomputed, yet its start
 * value is still added to it every frame. */
void battle_map_update_mesh_part_animations(void) {
    s32 i;
    u8 next;
    u8 anim;
    s16 duration;
    s16 time;
    SVECTOR* current_b;
    SVECTOR* start_b;

    /* Copying translation through this invariant pointer to part 0, with the
     * destination taken first, matches the target: it hoists
     * &g_map_mesh_parts[0].translation and bases the strength-reduced pointers on
     * it. */
    current_b = &g_map_mesh_parts[0].translation;

    for (i = 1; i < 9; i++) {
        /* Advance the timer. A keyframe that has run its length asks for the
         * next instruction; one of duration 0 ends the animation. */
        if (g_map_mesh_animation_instructions[i].states[0][1].next != 0) {
            g_map_mesh_parts[i].elapsed += (g_map_mesh_parts[i].speed >> 4) * g_main_system_animation_speed;
            if (g_map_mesh_parts[i].duration < g_map_mesh_parts[i].elapsed) {
                next = g_map_mesh_parts[i].next_instruction;
                g_map_mesh_parts[i].keyframe = 0;
                if (g_map_mesh_animation_instructions[i].states[0][next].duration == 0) {
                    g_map_mesh_part_animation_states[i] = 0;
                }
            }
        }
        if (g_map_mesh_part_animation_states[i] != 0) {
            /* Start the next keyframe: the current values become the start
             * values and each channel takes its delta. */
            if (g_map_mesh_parts[i].keyframe == 0) {
                g_map_mesh_parts[i].keyframe = g_map_mesh_animation_instructions[i].states[0][next].keyframe;
                anim = g_map_mesh_parts[i].keyframe;
                g_map_mesh_parts[i].next_instruction = g_map_mesh_animation_instructions[i].states[0][next].next;
                g_map_mesh_parts[i].duration = g_map_mesh_animation_instructions[i].states[0][next].duration;
                g_map_mesh_parts[i].rotation_start = g_map_mesh_parts[i].rotation;
                g_map_mesh_parts[i].scale_start = g_map_mesh_parts[i].scale;
                start_b = &g_map_mesh_parts[i].translation_start;
                *start_b = current_b[i * (sizeof(map_mesh_part_t) / sizeof(SVECTOR))];
                g_map_mesh_parts[i].elapsed = 1;
                if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vx
                        = g_map_mesh_animation_keyframes[anim].rotation.vx - g_map_mesh_parts[i].rotation_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vy
                        = g_map_mesh_animation_keyframes[anim].rotation.vy - g_map_mesh_parts[i].rotation_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vz
                        = g_map_mesh_animation_keyframes[anim].rotation.vz - g_map_mesh_parts[i].rotation_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vx = g_map_mesh_animation_keyframes[anim].translation.vx
                        - g_map_mesh_parts[i].translation_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vy = g_map_mesh_animation_keyframes[anim].translation.vy
                        - g_map_mesh_parts[i].translation_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vz = g_map_mesh_animation_keyframes[anim].translation.vz
                        - g_map_mesh_parts[i].translation_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vx
                        = g_map_mesh_animation_keyframes[anim].scale_x - g_map_mesh_parts[i].scale_start.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vy
                        = g_map_mesh_animation_keyframes[anim].scale_y - g_map_mesh_parts[i].scale_start.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vz
                        = g_map_mesh_animation_keyframes[anim].scale_z - g_map_mesh_parts[i].scale_start.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vx = g_map_mesh_animation_keyframes[anim].rotation.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vy = g_map_mesh_animation_keyframes[anim].rotation.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vz = g_map_mesh_animation_keyframes[anim].rotation.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vx = g_map_mesh_animation_keyframes[anim].translation.vx;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vy = g_map_mesh_animation_keyframes[anim].translation.vy;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vz = g_map_mesh_animation_keyframes[anim].translation.vz;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vx = g_map_mesh_animation_keyframes[anim].scale_x;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vy = g_map_mesh_animation_keyframes[anim].scale_y;
                }
                if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vz = g_map_mesh_animation_keyframes[anim].scale_z;
                }
            }
            /* Evaluate each channel's delta at elapsed / duration on its
             * curve: linear, sine or raised sine. */
            anim = g_map_mesh_parts[i].keyframe;
            duration = g_map_mesh_parts[i].duration;
            time = g_map_mesh_parts[i].elapsed;
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz * time / duration;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[0]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[0])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[0])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[1]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[1])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[1])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[2]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[2])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[2])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[3]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[3])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[3])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[4]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[4])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[4])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[5]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[5])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[5])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[6]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[6])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[6])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[7]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[7])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[7])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * rsin((g_map_mesh_animation_keyframes[anim].angle_end[8]
                                   - g_map_mesh_animation_keyframes[anim].angle_start[8])
                                * time / duration
                            + g_map_mesh_animation_keyframes[anim].angle_start[8])
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[0]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[0])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[0])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[1]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[1])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[1])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[2]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[2])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[2])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[3]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[3])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[3])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[4]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[4])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[4])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[5]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[5])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[5])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[6]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[6])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[6])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[7]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[7])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[7])
                            + ONE)
                    >> 12;
            }
            if (g_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * (rsin((g_map_mesh_animation_keyframes[anim].angle_end[8]
                                    - g_map_mesh_animation_keyframes[anim].angle_start[8])
                                   * time / duration
                               + g_map_mesh_animation_keyframes[anim].angle_start[8])
                            + ONE)
                    >> 12;
            }
            /* Offset every channel by its start value. */
            g_map_mesh_parts[i].rotation.vx += g_map_mesh_parts[i].rotation_start.vx;
            g_map_mesh_parts[i].rotation.vy += g_map_mesh_parts[i].rotation_start.vy;
            g_map_mesh_parts[i].rotation.vz += g_map_mesh_parts[i].rotation_start.vz;
            g_map_mesh_parts[i].translation.vx += g_map_mesh_parts[i].translation_start.vx;
            g_map_mesh_parts[i].translation.vy += g_map_mesh_parts[i].translation_start.vy;
            g_map_mesh_parts[i].translation.vz += g_map_mesh_parts[i].translation_start.vz;
            g_map_mesh_parts[i].scale.vx += g_map_mesh_parts[i].scale_start.vx;
            g_map_mesh_parts[i].scale.vy += g_map_mesh_parts[i].scale_start.vy;
            g_map_mesh_parts[i].scale.vz += g_map_mesh_parts[i].scale_start.vz;
        }
    }
}

/* Map-state command dispatcher.
 *
 * For event Use3DObject (0x80) `index` is the object id, `object_state` its
 * state row and `flag` is stored beside it; 0x9d reuses `object_state` as a
 * duration. */
s32 battle_map_dispatch_map_data_command(map_data_command_e command, u32 index, s32 object_state, s32 flag) {
    s32 result;
    s16 i;
    map_mesh_instructions_t* entry;

    switch (command) {
    case MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE:
        result = g_map_texture_animations[index].active;
        break;
    case MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_map_texture_animations[index].mode == MAP_TEXTURE_ANIMATION_MODE_NONE) {
            if (g_map_saved_texture_animation_modes[index] != 0) {
                g_map_texture_animations[index].mode = g_map_saved_texture_animation_modes[index];
            }
        }
        switch (g_map_texture_animations[index].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP ... MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK ... MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_map_texture_animation_moves[index].sx = g_map_texture_animations[index].frame_x;
            g_map_texture_animation_moves[index].sy = g_map_texture_animations[index].frame_y;
            g_map_texture_animation_moves[index].w = g_map_texture_animations[index].width;
            g_map_texture_animation_moves[index].h = g_map_texture_animations[index].height;
            g_map_texture_animation_moves[index].x0 = g_map_texture_animations[index].canvas_x;
            g_map_texture_animation_moves[index].y0 = g_map_texture_animations[index].canvas_y;
            g_map_texture_animations[index].active = 1;
            g_map_texture_animations[index].timer = 0x7e;
            g_map_texture_animations[index].frame = 0;
            break;
        default:
            main_system_handle_malloc_exception(2, 0x3cd);
            break;
        }
        break;
    case MAP_DATA_COMMAND_GET_3D_OBJECT_STATE:
        result = g_map_mesh_part_animation_states[index];
        break;
    case MAP_DATA_COMMAND_SET_3D_OBJECT_STATE:
        entry = &g_map_mesh_animation_instructions[index];
        for (i = 0; i < 17; i++) {
            entry->states[0][i] = entry->states[object_state][i];
        }
        g_map_mesh_parts[index].elapsed = 2;
        g_map_mesh_parts[index].duration = 0;
        g_map_mesh_parts[index].next_instruction = 1;
        g_map_mesh_part_animation_states[index] = (object_state << 8) | flag;
        break;
    case MAP_DATA_COMMAND_SPLIT_MESH_PARTS:
        for (i = 1; i < 9; i++) {
            g_map_mesh_parts[i].textured_triangle_count = (g_map_primary_textured_triangle_count - 1) / 8 + 1;
            g_map_mesh_parts[i].textured_quad_count = (g_map_primary_textured_quad_count - 1) / 8 + 1;
            g_map_mesh_parts[i].untextured_triangle_count = (g_map_primary_untextured_triangle_count - 1) / 8 + 1;
            g_map_mesh_parts[i].untextured_quad_count = (g_map_primary_untextured_quad_count - 1) / 8 + 1;
            g_map_mesh_parts[i].textured_triangle_start
                = ((g_map_primary_textured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].textured_quad_start = ((g_map_primary_textured_quad_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].untextured_triangle_start
                = ((g_map_primary_untextured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_map_mesh_parts[i].untextured_quad_start = ((g_map_primary_untextured_quad_count - 1) / 8 + 1) * (i - 1);
        }
        g_map_mesh_parts[0].textured_triangle_count = 0;
        g_map_mesh_parts[0].textured_quad_count = 0;
        g_map_mesh_parts[0].untextured_triangle_count = 0;
        g_map_mesh_parts[0].untextured_quad_count = 0;
        g_map_mesh_parts[0].textured_triangle_start = 0;
        g_map_mesh_parts[0].textured_quad_start = 0;
        g_map_mesh_parts[0].untextured_triangle_start = 0;
        g_map_mesh_parts[0].untextured_quad_start = 0;
        break;
    case MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER:
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    case MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER:
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    case MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_saved_texture_animation_modes[i] = g_map_texture_animations[i].mode;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_texture_animations[i].mode = g_map_saved_texture_animation_modes[i];
        }
        break;
    case MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_saved_texture_animation_modes[index] = g_map_texture_animations[index].mode;
            g_map_texture_animations[index].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_texture_animations[index].mode = g_map_saved_texture_animation_modes[index];
        }
        break;
    case MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_texture_animations[index].duration = object_state;
            g_map_texture_animations[index].timer = 0;
        }
        break;
    case MAP_DATA_COMMAND_RELEASE_GNS_HOLD:
        g_map_gns_dispatch_held = 0;
        break;
    }
    return result;
}

/*
 * Advance the 32 map texture animations once per frame.
 *
 * UV modes step the frame across the canvas row by row (1 loops, 2/0x80
 * ping-pong, 0x15-0x1C fast-forward to the last frame and then play 0x82 in
 * reverse, 5-0xC play once) and queue the DR_MOVE packet of the row;
 * palette modes (3 loops, 4/0x81 ping-pong, 0xD-0x14 play once) reload
 * CLUT canvas_x / 16 of the map row from palette frame frame_x + frame. Mode 0x1D
 * starts a rotation of the map row's colors canvas_x..canvas_x + width - 1.
 * Only the low byte of g_main_system_animation_speed is read (lbu), and the start call
 * passes the color range unmasked, as the target does.
 */
void battle_map_update_texture_animations(void) {
    s32 i;

    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        if (g_map_texture_animations[i].active == 0) {
            continue;
        }
        switch (g_map_texture_animations[i].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP:
            g_map_texture_animations[i].timer++;
            if (g_map_texture_animations[i].timer < g_map_texture_animations[i].duration) {
                break;
            }
            g_map_texture_animations[i].timer = 0;
            g_map_texture_animations[i].frame++;
            g_map_texture_animations[i].column++;
            if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row++;
                g_map_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
            }
            if (g_map_texture_animations[i].frame_count - 1 < g_map_texture_animations[i].frame) {
                g_map_texture_animations[i].frame = 0;
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row = 0;
                g_map_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x - g_map_texture_animations[i].width;
            }
            g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
            g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
            AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame_count - 2 < g_map_texture_animations[i].frame) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK;
                }
                g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                if (g_map_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width;
                    g_map_texture_animations[i].row--;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].column * g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG;
                }
                g_map_texture_animation_moves[i].sx -= g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP:
            g_map_texture_animations[i].timer++;
            if (g_map_texture_animations[i].timer < g_map_texture_animations[i].duration) {
                break;
            }
            g_map_texture_animations[i].timer = 0;
            g_map_texture_animations[i].frame++;
            g_map_texture_animations[i].column++;
            if (g_map_texture_animations[i].frame_count - 1 < g_map_texture_animations[i].frame) {
                g_map_texture_animations[i].frame = 0;
                g_map_texture_animations[i].column = 0;
            }
            battle_map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].column,
                MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count - 1) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK;
                }
                battle_map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG;
                }
                battle_map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_7:
            g_map_texture_animations[i].frame = 0;
            while (g_map_texture_animations[i].frame < g_map_texture_animations[i].frame_count) {
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                g_map_texture_animations[i].frame++;
            }
            g_map_texture_animations[i].timer = 0x7E;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING;
            /* fall through */
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                if (g_map_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width - 1;
                    g_map_texture_animations[i].row--;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].active = 0;
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0;
                }
                g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].width * g_map_texture_animations[i].column;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_7:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                if (g_map_texture_animations[i].column != 0) {
                    g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                }
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    g_map_texture_animation_moves[i].sy
                        += g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                }
                if (g_map_texture_animations[i].frame > g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].frame = 0;
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row = 0;
                    g_map_texture_animations[i].active = 0;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x;
                    g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_7:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                battle_map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].active = 0;
                }
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
            if (g_map_texture_animation_cycle_handles[i] == 0) {
                g_map_texture_animation_cycle_handles[i] = battle_map_start_clut_cycle(
                    g_map_texture_animations[i].duration, MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x,
                    g_map_texture_animations[i].canvas_x + g_map_texture_animations[i].width - 1);
                if (g_map_texture_animation_cycle_handles[i] == 0) {
                    main_system_handle_malloc_exception(2, 0x3CC);
                }
            }
            break;
        }
    }
}

/* Loads one record of the saved map state and returns the address just past it.
 *
 * Type 1 fills entries 1..count of the 0x50-byte table, type 2 the 16 cells of
 * each of 8 rows for entries 1..8 of the 0x220-byte table, and type 3 sets up
 * the 0x98-byte entries 1..8 from the other two tables (4 input bytes per
 * entry, extra entries skipped). The caller at 0x800f32e4 stops at type 0
 * before calling, so the result for type 0 or an unknown type is never used;
 * `next` is left unset there, which is why the target returns $a1. */
map_state_record_t* battle_map_load_saved_state_record(map_state_record_t* record) {
    map_state_record_t* next;
    u32 count;
    u32 i;
    u32 y;
    u32 c;

    count = record->count;
    switch (record->type) {
    case MAP_MESH_ANIMATION_RECORD_KEYFRAMES: {
        map_mesh_keyframe_t* src = (map_mesh_keyframe_t*)record->data;

        count++;
        for (i = 1; i < count; i++) {
            g_map_mesh_animation_keyframes[i] = *src++;
        }
        next = (map_state_record_t*)src;
        break;
    }
    case MAP_MESH_ANIMATION_RECORD_END:
        break;
    case MAP_MESH_ANIMATION_RECORD_INSTRUCTIONS: {
        map_mesh_instruction_t* src = (map_mesh_instruction_t*)record->data;

        for (y = 0; y < 8; y++) {
            for (i = 1; i < 9; i++) {
                for (c = 1; c < 17; c++) {
                    g_map_mesh_animation_instructions[i].states[y][c] = *src++;
                }
            }
        }
        next = (map_state_record_t*)src;
        break;
    }
    case MAP_MESH_ANIMATION_RECORD_PART_PROPERTIES: {
        u8* src = record->data;

        count++;
        for (i = 1; i < count; i++) {
            if (i >= 9) {
                break;
            }
            g_map_mesh_parts[i].parent = *src;
            g_map_mesh_parts[i].duration = 0;
            g_map_mesh_parts[i].elapsed = 2;
            src += 4;
            if (g_map_mesh_animation_instructions[i].states[0][1].next != 0) {
                g_map_mesh_part_animation_states[i] = 1;
            } else {
                g_map_mesh_part_animation_states[i] = 0;
            }
            g_map_mesh_parts[i].keyframe = 0;
            g_map_mesh_parts[i].rotation
                = g_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].rotation;
            g_map_mesh_parts[i].translation
                = g_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe]
                      .translation;
            g_map_mesh_parts[i].scale.vx
                = g_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale_x;
            g_map_mesh_parts[i].scale.vy
                = g_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale_y;
            g_map_mesh_parts[i].scale.vz
                = g_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale_z;
        }
        for (; i < count; i++) {
            src += 4;
        }
        next = (map_state_record_t*)src;
        break;
    }
    }
    return next;
}

/* 1 when the requested map state stands in the relation to a GNS row's state that
 * the row's comparison byte asks for: equal (1), requested less than the row's
 * (2) or requested greater (3). Any other comparison never matches. Every file
 * row of the retail GNS files uses 1. */
s16 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state) {
    s32 matches;

    comparison &= 0xff;
    matches = 0;
    switch (comparison) {
    case GNS_MAP_STATE_EQUAL: {
        if (requested_state == record_state) {
            matches = 1;
        }
        break;
    }
    case GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD: {
        if (requested_state < record_state) {
            matches = 1;
        }
        break;
    }
    case GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD: {
        if (requested_state > record_state) {
            matches = 1;
        }
        break;
    }
    }
    return matches;
}

/* Apply one map resource by type: mesh parts, polygon flags, palettes,
 * textures, lighting, texture animations, terrain, saved state, camera and
 * weather commands. The three mesh file types walk the file's table of chunk
 * offsets and dispatch each present entry as types 0x10-0x30. */
s32 battle_map_dispatch_gns_resource(u8 resource_type, map_resource_t* data) {
    MATRIX direction;
    SVECTOR zero_vector;
    VECTOR unit_scale;
    map_texture_animation_t* source;
    map_light_resource_t* light;
    u16* polygon_flags;
    /* 16-bit cursor of the light, camera and weather-modifier cases.
     * Direct command payload-vector access removes four bytes from each arm. */
    s16* vector;
    map_state_record_t* cursor;
    s32 result;
    s32 i;
    u16 flags;
    u8 red;
    u8 green;
    u8 blue;

    zero_vector = g_map_zero_svector;
    unit_scale = g_map_unit_scale_vector;
    while (DrawSync(1) != 0) { }
    switch (resource_type) {
    case MAP_RESOURCE_GEOMETRY_PRIMARY:
        g_map_textured_triangle_count = 0;
        g_map_textured_quad_count = 0;
        g_map_untextured_triangle_count = 0;
        g_map_untextured_quad_count = 0;
        for (i = 0; i < 9; i++) {
            g_map_mesh_parts[i].textured_triangle_count = 0;
            g_map_mesh_parts[i].textured_quad_count = 0;
            g_map_mesh_parts[i].untextured_triangle_count = 0;
            g_map_mesh_parts[i].untextured_quad_count = 0;
            g_map_mesh_part_geometry[i] = 0;
        }
        g_map_mesh_part_geometry[0] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[0]);
        g_map_primary_textured_triangle_count = g_map_textured_triangle_count;
        g_map_primary_textured_quad_count = g_map_textured_quad_count;
        g_map_primary_untextured_triangle_count = g_map_untextured_triangle_count;
        g_map_primary_untextured_quad_count = g_map_untextured_quad_count;
        break;
    case MAP_RESOURCE_GEOMETRY_PART_1:
        g_map_mesh_part_geometry[1] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[1]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_2:
        g_map_mesh_part_geometry[2] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[2]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_3:
        g_map_mesh_part_geometry[3] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[3]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_4:
        g_map_mesh_part_geometry[4] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[4]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_5:
        g_map_mesh_part_geometry[5] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[5]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_6:
        g_map_mesh_part_geometry[6] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[6]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_7:
        g_map_mesh_part_geometry[7] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[7]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_8:
        g_map_mesh_part_geometry[8] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[8]);
        break;
    case MAP_RESOURCE_POLYGON_FLAGS:
        polygon_flags = data->polygon_flags.textured_triangles;
        for (i = 0; i < 360; i++) {
            flags = *polygon_flags++;
            g_map_textured_triangle_positions[i].polygon_flags = flags;
            if (!(flags & MAP_POLYGON_FLAG_LIT)) {
                g_battle_render_buffers[0].gt3[i].r0 = 0x80;
                g_battle_render_buffers[0].gt3[i].r1 = 0x80;
                g_battle_render_buffers[0].gt3[i].r2 = 0x80;
                g_battle_render_buffers[0].gt3[i].g0 = 0x80;
                g_battle_render_buffers[0].gt3[i].g1 = 0x80;
                g_battle_render_buffers[0].gt3[i].g2 = 0x80;
                g_battle_render_buffers[0].gt3[i].b0 = 0x80;
                g_battle_render_buffers[0].gt3[i].b1 = 0x80;
                g_battle_render_buffers[0].gt3[i].b2 = 0x80;
                g_battle_render_buffers[1].gt3[i].r0 = 0x80;
                g_battle_render_buffers[1].gt3[i].r1 = 0x80;
                g_battle_render_buffers[1].gt3[i].r2 = 0x80;
                g_battle_render_buffers[1].gt3[i].g0 = 0x80;
                g_battle_render_buffers[1].gt3[i].g1 = 0x80;
                g_battle_render_buffers[1].gt3[i].g2 = 0x80;
                g_battle_render_buffers[1].gt3[i].b0 = 0x80;
                g_battle_render_buffers[1].gt3[i].b1 = 0x80;
                g_battle_render_buffers[1].gt3[i].b2 = 0x80;
            }
        }
        for (; i < 0x200; i++) {
            polygon_flags++;
        }
        for (i = 0; i < 710; i++) {
            flags = *polygon_flags++;
            g_map_textured_quad_positions[i].polygon_flags = flags;
            if (!(flags & MAP_POLYGON_FLAG_LIT)) {
                g_battle_render_buffers[0].gt4[i].r0 = 0x80;
                g_battle_render_buffers[0].gt4[i].r1 = 0x80;
                g_battle_render_buffers[0].gt4[i].r2 = 0x80;
                g_battle_render_buffers[0].gt4[i].r3 = 0x80;
                g_battle_render_buffers[0].gt4[i].g0 = 0x80;
                g_battle_render_buffers[0].gt4[i].g1 = 0x80;
                g_battle_render_buffers[0].gt4[i].g2 = 0x80;
                g_battle_render_buffers[0].gt4[i].g3 = 0x80;
                g_battle_render_buffers[0].gt4[i].b0 = 0x80;
                g_battle_render_buffers[0].gt4[i].b1 = 0x80;
                g_battle_render_buffers[0].gt4[i].b2 = 0x80;
                g_battle_render_buffers[0].gt4[i].b3 = 0x80;
                g_battle_render_buffers[1].gt4[i].r0 = 0x80;
                g_battle_render_buffers[1].gt4[i].r1 = 0x80;
                g_battle_render_buffers[1].gt4[i].r2 = 0x80;
                g_battle_render_buffers[1].gt4[i].r3 = 0x80;
                g_battle_render_buffers[1].gt4[i].g0 = 0x80;
                g_battle_render_buffers[1].gt4[i].g1 = 0x80;
                g_battle_render_buffers[1].gt4[i].g2 = 0x80;
                g_battle_render_buffers[1].gt4[i].g3 = 0x80;
                g_battle_render_buffers[1].gt4[i].b0 = 0x80;
                g_battle_render_buffers[1].gt4[i].b1 = 0x80;
                g_battle_render_buffers[1].gt4[i].b2 = 0x80;
                g_battle_render_buffers[1].gt4[i].b3 = 0x80;
            }
        }
        for (; i < 0x300; i++) {
            polygon_flags++;
        }
        for (i = 0; i < 64; i++) {
            g_map_untextured_triangle_positions[i].polygon_flags = *polygon_flags++;
        }
        for (i = 0; i < 256; i++) {
            g_map_untextured_quad_positions[i].polygon_flags = *polygon_flags++;
        }
        battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        break;
    case MAP_RESOURCE_PALETTES:
        g_map_palette_pending = 1;
        for (i = 0; i < 0x200; i++) {
            g_map_pending_palette_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_PALETTES:
        g_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MAP;
        g_map_vram_load_rect.w = 0x100;
        g_map_palette_pending = 0;
        g_map_vram_load_rect.x = 0;
        g_map_vram_load_rect.h = 1;
        battle_map_load_palette_data(data->clut, MAP_CLUT_ROW_MAP, 0, 1);
        break;
    case MAP_RESOURCE_MARKED_TILE_PALETTES:
        g_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MARKED_TILES;
        g_map_vram_load_rect.w = 0x100;
        g_map_vram_load_rect.x = 0;
        g_map_vram_load_rect.h = 1;
        battle_map_load_palette_data(data->clut, MAP_CLUT_ROW_MARKED_TILES, 0, 1);
        break;
    case MAP_RESOURCE_UNSUPPORTED_12:
        main_system_handle_malloc_exception(2, 0x3ca);
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_0:
        g_map_vram_load_rect.x = 0x300;
        g_map_vram_load_rect.w = 0x40;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 0x100;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_1:
        g_map_vram_load_rect.x = 0x340;
        g_map_vram_load_rect.w = 0x40;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 0x100;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_2:
        g_map_vram_load_rect.x = 0x380;
        g_map_vram_load_rect.w = 0x40;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 0x100;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_3:
        g_map_vram_load_rect.x = 0x3c0;
        g_map_vram_load_rect.w = 0x40;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 0x100;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE:
        if (g_map_texture_quarter_0_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_0, data);
        }
        if (g_map_texture_quarter_1_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_1, (map_resource_t*)(data->bytes + 0x8000));
        }
        if (g_map_texture_quarter_2_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_2, (map_resource_t*)(data->bytes + 0x10000));
        }
        if (g_map_texture_quarter_3_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_3, (map_resource_t*)(data->bytes + 0x18000));
        }
        g_map_texture_quarter_0_pending = 0;
        g_map_texture_quarter_1_pending = 0;
        g_map_texture_quarter_2_pending = 0;
        g_map_texture_quarter_3_pending = 0;
        g_map_texture_pending = 0;
        break;
    case MAP_RESOURCE_MESH_PRIMARY:
    case MAP_RESOURCE_MESH_OVERRIDE:
    case MAP_RESOURCE_MESH_ALTERNATE:
        for (i = MAP_RESOURCE_GEOMETRY_PRIMARY; i < GNS_RESOURCE_TABLE_COUNT; i++) {
            if (data->mesh_file[i] != 0) {
                battle_map_dispatch_gns_resource(i, (map_resource_t*)(data->bytes + data->mesh_file[i]));
            }
        }
        break;
    case MAP_RESOURCE_LIGHTING:
        g_map_lighting_pending = 1;
        for (i = 0; i < 0x2d; i++) {
            g_map_pending_lighting_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_LIGHTING:
        light = &data->lighting;
        g_map_lighting_pending = 0;
        setVector(&g_map_light_rotation, 0, 0, 0);
        vector = light->color[0];
        vector += 9;
        direction.m[0][0] = vector[0];
        direction.m[0][1] = vector[1];
        direction.m[0][2] = vector[2];
        direction.m[1][0] = vector[3];
        direction.m[1][1] = vector[4];
        direction.m[1][2] = vector[5];
        direction.m[2][0] = vector[6];
        direction.m[2][1] = vector[7];
        direction.m[2][2] = vector[8];
        /* Retail reads the packed direction bytes as MATRIX translation fields. */
        battle_map_set_light_matrices((MATRIX*)light, &direction);
        battle_map_control_gte_background_color(
            BATTLE_BACKGROUND_COLOR_SET, light->ambient.r, light->ambient.g, light->ambient.b);
        red = light->gradient_top.r;
        green = light->gradient_top.g;
        blue = light->gradient_top.b;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r = red;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g = green;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b = blue;
        red = light->gradient_bottom.r;
        green = light->gradient_bottom.g;
        blue = light->gradient_bottom.b;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = red;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = green;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = blue;
        g_map_lightning_state.base_colors[0] = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_map_lightning_state.base_colors[1] = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
        break;
    case MAP_RESOURCE_TEXTURE_ANIMATIONS:
        source = data->texture_animations;
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_texture_animation_cycle_handles[i] = 0;
            g_map_texture_animations[i] = source[i];
            SetDrawMove(&g_map_texture_animation_moves[i]);
            g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x;
            g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y;
            g_map_texture_animation_moves[i].w = g_map_texture_animations[i].width;
            g_map_texture_animation_moves[i].h = g_map_texture_animations[i].height;
            g_map_texture_animation_moves[i].x0 = g_map_texture_animations[i].canvas_x;
            g_map_texture_animation_moves[i].y0 = g_map_texture_animations[i].canvas_y;
            if (g_map_texture_animations[i].mode >= MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0
                && g_map_texture_animations[i].mode < MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE) {
                g_map_texture_animations[i].active = 0;
            } else if (g_map_texture_animations[i].mode != MAP_TEXTURE_ANIMATION_MODE_NONE) {
                g_map_texture_animations[i].active = 1;
            }
        }
        break;
    case MAP_RESOURCE_PALETTE_ANIMATION_FRAMES:
        for (i = 0; i < 0x100; i++) {
            /* Palette-bank indexing changes the native 16-bit copy loop. */
            ((u16*)g_map_palette_animation_frames)[i] = ((u16*)data->clut)[i];
        }
        break;
    case MAP_RESOURCE_TERRAIN: {
        map_terrain_resource_t* terrain = &data->terrain;
        g_map_tile_width = terrain->width;
        g_map_tile_depth = terrain->depth;
        battle_map_copy_xy_coords_and_tile_data(data->bytes);
        break;
    }
    case MAP_RESOURCE_STATE_RECORDS:
        cursor = &data->state_record;
        if (cursor->type != MAP_MESH_ANIMATION_RECORD_END) {
            do {
                cursor = battle_map_load_saved_state_record(cursor);
            } while (cursor->type != MAP_MESH_ANIMATION_RECORD_END);
        }
        break;
    case GNS_COMMAND_SET_CAMERA_POSITION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_camera_integer_coords, -vector[0], vector[1], -vector[2]);
        battle_camera_set_focus_and_cursor_tile(&g_camera_integer_coords, &g_map_camera_rotation);
        break;
    case GNS_COMMAND_SET_CAMERA_ROTATION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_map_camera_rotation, vector[0], vector[1], vector[2]);
        battle_camera_set_focus_and_cursor_tile(&g_camera_integer_coords, &g_map_camera_rotation);
        break;
    /* Case-local pointers preserve the native address lifetimes. */
    case GNS_COMMAND_SET_INDOOR: {
        s32* weather_flags;

        weather_flags = &g_map_weather_flags;
        *weather_flags |= MAP_WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    }
    case GNS_COMMAND_SET_OUTDOOR: {
        s32* weather_flags;

        weather_flags = &g_map_weather_flags;
        *weather_flags &= ~MAP_WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    }
    case GNS_COMMAND_CLEAR_SNOW: {
        s32* weather_flags;

        weather_flags = &g_map_weather_flags;
        *weather_flags &= ~MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_SNOW: {
        s32* weather_flags;

        weather_flags = &g_map_weather_flags;
        *weather_flags |= MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_WEATHER_MODIFIER: {
        s32* weather_flags;

        vector = data->command_halfwords;
        vector += 6;
        weather_flags = &g_map_weather_flags;
        *weather_flags = (*weather_flags & MAP_WEATHER_LOW_FLAGS_MASK) | (vector[0] << MAP_WEATHER_MODIFIER_SHIFT);
        break;
    }
    }
    return result;
}

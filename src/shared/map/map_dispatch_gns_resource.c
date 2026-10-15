#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Apply one map resource by type: mesh parts, polygon flags, palettes,
 * textures, lighting, texture animations, terrain, saved state, camera and
 * weather commands. The three mesh file types walk the file's table of chunk
 * offsets and dispatch each present entry as types 0x10-0x30. */
s32 map_dispatch_gns_resource(u8 resource_type, map_resource_t* data) {
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
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[0]);
        g_map_primary_textured_triangle_count = g_map_textured_triangle_count;
        g_map_primary_textured_quad_count = g_map_textured_quad_count;
        g_map_primary_untextured_triangle_count = g_map_untextured_triangle_count;
        g_map_primary_untextured_quad_count = g_map_untextured_quad_count;
        break;
    case MAP_RESOURCE_GEOMETRY_PART_1:
        g_map_mesh_part_geometry[1] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[1]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_2:
        g_map_mesh_part_geometry[2] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[2]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_3:
        g_map_mesh_part_geometry[3] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[3]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_4:
        g_map_mesh_part_geometry[4] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[4]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_5:
        g_map_mesh_part_geometry[5] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[5]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_6:
        g_map_mesh_part_geometry[6] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[6]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_7:
        g_map_mesh_part_geometry[7] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[7]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_8:
        g_map_mesh_part_geometry[8] = &data->geometry;
        map_append_mesh_geometry(&data->geometry, &g_map_mesh_parts[8]);
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
        map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
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
        map_load_palette_data(data->clut, MAP_CLUT_ROW_MAP, 0, 1);
        break;
    case MAP_RESOURCE_MARKED_TILE_PALETTES:
        g_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MARKED_TILES;
        g_map_vram_load_rect.w = 0x100;
        g_map_vram_load_rect.x = 0;
        g_map_vram_load_rect.h = 1;
        map_load_palette_data(data->clut, MAP_CLUT_ROW_MARKED_TILES, 0, 1);
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
            map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_0, data);
        }
        if (g_map_texture_quarter_1_pending != 0) {
            map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_1, (map_resource_t*)(data->bytes + 0x8000));
        }
        if (g_map_texture_quarter_2_pending != 0) {
            map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_2, (map_resource_t*)(data->bytes + 0x10000));
        }
        if (g_map_texture_quarter_3_pending != 0) {
            map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_3, (map_resource_t*)(data->bytes + 0x18000));
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
                map_dispatch_gns_resource(i, (map_resource_t*)(data->bytes + data->mesh_file[i]));
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
        map_set_light_matrices((MATRIX*)light, &direction);
        map_control_gte_background_color(
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
        map_light_state_command(MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
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
        map_copy_xy_coords_and_tile_data(data->bytes);
        break;
    }
    case MAP_RESOURCE_STATE_RECORDS:
        cursor = &data->state_record;
        if (cursor->type != MAP_MESH_ANIMATION_RECORD_END) {
            do {
                cursor = map_load_saved_state_record(cursor);
            } while (cursor->type != MAP_MESH_ANIMATION_RECORD_END);
        }
        break;
    case GNS_COMMAND_SET_CAMERA_POSITION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_camera_integer_coords, -vector[0], vector[1], -vector[2]);
        camera_set_focus_and_cursor_tile(&g_camera_integer_coords, &g_map_camera_rotation);
        break;
    case GNS_COMMAND_SET_CAMERA_ROTATION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_map_camera_rotation, vector[0], vector[1], vector[2]);
        camera_set_focus_and_cursor_tile(&g_camera_integer_coords, &g_map_camera_rotation);
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

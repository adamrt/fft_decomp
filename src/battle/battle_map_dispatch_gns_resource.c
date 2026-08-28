#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

typedef struct mesh_triangle_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
} mesh_triangle_positions_t;

typedef struct mesh_quad_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} mesh_quad_positions_t;

/* Bit view of the map weather flag word at 0x800b6698. */
typedef struct battle_map_weather_bits {
    u32 snow : 1;
    u32 ignore_weather : 1;
    u32 unknown_2 : 5;
    s32 modifier : 25;
} battle_map_weather_bits_t;

#define WEATHER ((battle_map_weather_bits_t*)&g_battle_map_weather_flags)

/* Callee-side view, as declared by battle_map_append_mesh_geometry.c. */
typedef struct map_mesh_part_metadata_t {
    u8 _unknown00[0x88];
    u16 textured_triangle_start;
    u16 textured_quad_start;
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    u16 textured_triangle_count;
    u16 textured_quad_count;
    u16 untextured_triangle_count;
    u16 untextured_quad_count;
} map_mesh_part_metadata_t;

/* Lighting resource (GNS resource 0x32): the light colour matrix, the light
 * direction matrix, then the ambient colour and the background gradient's top
 * and bottom colours. The colours come first: 0x00 is passed as the colour
 * matrix to battle_map_set_light_matrices and 0x24 to the GTE back colour. */
typedef struct battle_map_light_resource {
    s16 colour[3][3];      /* 0x00 */
    s16 direction[3][3];   /* 0x12 */
    u8 ambient[3];         /* 0x24 */
    u8 gradient_top[3];    /* 0x27 */
    u8 gradient_bottom[3]; /* 0x2a */
} battle_map_light_resource_t;

extern mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern mesh_quad_positions_t g_battle_map_textured_quad_positions[];
extern mesh_triangle_positions_t g_battle_map_untextured_triangle_positions[];
extern mesh_quad_positions_t g_battle_map_untextured_quad_positions[];

extern void battle_map_append_mesh_geometry(u16* geometry_data, map_mesh_part_metadata_t* metadata);

/* Apply one map resource by type: mesh parts, polygon flags, palettes,
 * textures, lighting, texture animations, terrain, saved state, camera and
 * weather commands. Types 0x2e-0x30 walk a table of resource offsets and
 * dispatch each present entry as types 0x10-0x30. */
s32 battle_map_dispatch_gns_resource(u8 resource_type, u8* data) {
    MATRIX direction;
    SVECTOR zero_vector;
    VECTOR unit_scale;
    battle_map_texture_animation_t* source;
    battle_map_texture_animation_t* animation;
    battle_map_light_resource_t* light;
    u16* polygon_flags;
    /* One pointer shared by the light and camera cases; being referenced in
     * several blocks puts it in global allocation (a block-local copy of
     * data + offset gives `data` a $v1 preference that later steals $v1 from
     * the case 0x2c loop). */
    s16* vector;
    u32* resource_offset;
    battle_map_state_record_t* cursor;
    s32 result;
    s32 i;
    u16 flags;
    u8 red;
    u8 green;
    u8 blue;

    zero_vector = g_battle_map_zero_svector;
    unit_scale = g_battle_map_unit_scale_vector;
    while (DrawSync(1) != 0) { }
    switch (resource_type) {
    case 0x10:
        g_battle_map_textured_triangle_count = 0;
        g_battle_map_textured_quad_count = 0;
        g_battle_map_untextured_triangle_count = 0;
        g_battle_map_untextured_quad_count = 0;
        for (i = 0; i < 9; i++) {
            g_battle_map_mesh_parts[i].counts[0] = 0;
            g_battle_map_mesh_parts[i].counts[1] = 0;
            g_battle_map_mesh_parts[i].counts[2] = 0;
            g_battle_map_mesh_parts[i].counts[3] = 0;
            g_battle_map_mesh_part_geometry[i] = 0;
        }
        g_battle_map_mesh_part_geometry[0] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[0]);
        g_battle_map_primary_textured_triangle_count = g_battle_map_textured_triangle_count;
        g_battle_map_primary_textured_quad_count = g_battle_map_textured_quad_count;
        g_battle_map_primary_untextured_triangle_count = g_battle_map_untextured_triangle_count;
        g_battle_map_primary_untextured_quad_count = g_battle_map_untextured_quad_count;
        break;
    case 0x24:
        g_battle_map_mesh_part_geometry[1] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[1]);
        break;
    case 0x25:
        g_battle_map_mesh_part_geometry[2] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[2]);
        break;
    case 0x26:
        g_battle_map_mesh_part_geometry[3] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[3]);
        break;
    case 0x27:
        g_battle_map_mesh_part_geometry[4] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[4]);
        break;
    case 0x28:
        g_battle_map_mesh_part_geometry[5] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[5]);
        break;
    case 0x29:
        g_battle_map_mesh_part_geometry[6] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[6]);
        break;
    case 0x2a:
        g_battle_map_mesh_part_geometry[7] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[7]);
        break;
    case 0x2b:
        g_battle_map_mesh_part_geometry[8] = data;
        battle_map_append_mesh_geometry((u16*)data, (map_mesh_part_metadata_t*)&g_battle_map_mesh_parts[8]);
        break;
    case 0x2c:
        polygon_flags = (u16*)(data + 0x380);
        for (i = 0; i < 360; i++) {
            flags = *polygon_flags++;
            g_battle_map_textured_triangle_positions[i]._pad0e = flags;
            if (!(flags & 0x8000)) {
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
            g_battle_map_textured_quad_positions[i]._pad0e = flags;
            if (!(flags & 0x8000)) {
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
            g_battle_map_untextured_triangle_positions[i]._pad0e = *polygon_flags++;
        }
        for (i = 0; i < 256; i++) {
            g_battle_map_untextured_quad_positions[i]._pad0e = *polygon_flags++;
        }
        battle_map_polygon_flag_command(0x46);
        break;
    case 0x11:
        g_battle_map_palette_pending = 1;
        for (i = 0; i < 0x200; i++) {
            g_battle_map_pending_palette_data[i] = data[i];
        }
        break;
    case 0x33:
        g_battle_map_vram_load_rect.y = 0x1e0;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_palette_pending = 0;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_load_palette_data((u16*)data, 0, 0, 1);
        break;
    case 0x1f:
        g_battle_map_vram_load_rect.y = 0x1e1;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_load_palette_data((u16*)data, 1, 0, 1);
        break;
    case 0x12:
        main_system_handle_malloc_exception(2, 0x3ca);
        break;
    case 0x13:
        g_battle_map_vram_load_rect.x = 0x300;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, (u32*)data);
        while (DrawSync(1) != 0) { }
        break;
    case 0x14:
        g_battle_map_vram_load_rect.x = 0x340;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, (u32*)data);
        while (DrawSync(1) != 0) { }
        break;
    case 0x15:
        g_battle_map_vram_load_rect.x = 0x380;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, (u32*)data);
        while (DrawSync(1) != 0) { }
        break;
    case 0x16:
        g_battle_map_vram_load_rect.x = 0x3c0;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, (u32*)data);
        while (DrawSync(1) != 0) { }
        break;
    case GNS_RESOURCE_TEXTURE:
        if (g_battle_map_texture_quarter_0_pending != 0) {
            battle_map_dispatch_gns_resource(0x13, data);
        }
        if (g_battle_map_texture_quarter_1_pending != 0) {
            battle_map_dispatch_gns_resource(0x14, data + 0x8000);
        }
        if (g_battle_map_texture_quarter_2_pending != 0) {
            battle_map_dispatch_gns_resource(0x15, data + 0x10000);
        }
        if (g_battle_map_texture_quarter_3_pending != 0) {
            battle_map_dispatch_gns_resource(0x16, data + 0x18000);
        }
        g_battle_map_texture_quarter_0_pending = 0;
        g_battle_map_texture_quarter_1_pending = 0;
        g_battle_map_texture_quarter_2_pending = 0;
        g_battle_map_texture_quarter_3_pending = 0;
        g_battle_map_texture_pending = 0;
        break;
    case GNS_RESOURCE_MESH_PRIMARY:
    case GNS_RESOURCE_MESH_OVERRIDE:
    case GNS_RESOURCE_MESH_ALTERNATE:
        i = 0x10;
        resource_offset = (u32*)(data + 0x40);
        for (; i < 0x31; i++) {
            if (*resource_offset != 0) {
                battle_map_dispatch_gns_resource(i, data + *resource_offset);
            }
            resource_offset++;
        }
        break;
    case 0x19:
        g_battle_map_lighting_pending = 1;
        for (i = 0; i < 0x2d; i++) {
            g_battle_map_pending_lighting_data[i] = data[i];
        }
        break;
    case 0x32:
        light = (battle_map_light_resource_t*)data;
        g_battle_map_lighting_pending = 0;
        g_battle_map_light_rotation.vx = 0;
        g_battle_map_light_rotation.vy = 0;
        g_battle_map_light_rotation.vz = 0;
        vector = light->direction[0];
        /* Keeps the direction pointer in its own register; CSE otherwise folds each load into data. */
        __asm__("" : "=r"(vector) : "0"(vector));
        direction.m[0][0] = vector[0];
        direction.m[0][1] = vector[1];
        direction.m[0][2] = vector[2];
        direction.m[1][0] = vector[3];
        direction.m[1][1] = vector[4];
        direction.m[1][2] = vector[5];
        direction.m[2][0] = vector[6];
        direction.m[2][1] = vector[7];
        direction.m[2][2] = vector[8];
        battle_map_set_light_matrices((MATRIX*)light, &direction);
        battle_map_control_gte_background_color(9, light->ambient[0], light->ambient[1], light->ambient[2]);
        red = light->gradient_top[0];
        green = light->gradient_top[1];
        blue = light->gradient_top[2];
        g_battle_map_background_gradient_colors.first.red = red;
        g_battle_map_background_gradient_colors.first.green = green;
        g_battle_map_background_gradient_colors.first.blue = blue;
        red = light->gradient_bottom[0];
        green = light->gradient_bottom[1];
        blue = light->gradient_bottom[2];
        g_battle_map_background_gradient_colors.second.red = red;
        g_battle_map_background_gradient_colors.second.green = green;
        g_battle_map_background_gradient_colors.second.blue = blue;
        g_battle_map_lightning_state.base_colors[0] = g_battle_map_background_gradient_colors.first;
        g_battle_map_lightning_state.base_colors[1] = g_battle_map_background_gradient_colors.second;
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
        break;
    case 0x1b:
        source = (battle_map_texture_animation_t*)data;
        for (i = 0; i < 32; i++) {
            g_battle_map_texture_animation_handles[i] = 0;
            g_battle_map_texture_animations[i] = source[i];
            SetDrawMove(&g_battle_map_texture_animation_moves[i]);
            g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x;
            g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y;
            g_battle_map_texture_animation_moves[i].w = g_battle_map_texture_animations[i].width;
            g_battle_map_texture_animation_moves[i].h = g_battle_map_texture_animations[i].height;
            g_battle_map_texture_animation_moves[i].x1 = g_battle_map_texture_animations[i].canvas_x;
            g_battle_map_texture_animation_moves[i].y1 = g_battle_map_texture_animations[i].canvas_y;
            if (g_battle_map_texture_animations[i].mode >= 5 && g_battle_map_texture_animations[i].mode < 0x1d) {
                g_battle_map_texture_animations[i].active = 0;
            } else if (g_battle_map_texture_animations[i].mode != 0) {
                g_battle_map_texture_animations[i].active = 1;
            }
        }
        break;
    case 0x1c:
        for (i = 0; i < 0x100; i++) {
            g_battle_map_palette_animation_frames[0][i] = ((u16*)data)[i];
        }
        break;
    case 0x1a:
        g_battle_map_tile_width = data[0];
        g_battle_map_tile_depth = data[1];
        battle_map_copy_xy_coords_and_tile_data(data);
        break;
    case 0x23:
        cursor = (battle_map_state_record_t*)data;
        if (cursor->type != 0) {
            do {
                cursor = (battle_map_state_record_t*)battle_map_load_saved_state_record(cursor);
            } while (cursor->type != 0);
        }
        break;
    case 0x83:
        vector = (s16*)(data + 0xc);
        /* Keeps data + 0xc in its own register; CSE otherwise folds 0xc(data). */
        __asm__("" : "=r"(vector) : "0"(vector));
        g_battle_camera_integer_coords.vx = -vector[0];
        g_battle_camera_integer_coords.vy = vector[1];
        g_battle_camera_integer_coords.vz = -vector[2];
        battle_camera_set_focus_and_cursor_tile((battle_screen_coords_t*)&g_battle_camera_integer_coords,
            (battle_camera_rotation_t*)&g_battle_map_camera_rotation);
        break;
    case 0x84:
        vector = (s16*)(data + 0xc);
        /* Keeps data + 0xc in its own register; CSE otherwise folds 0xc(data). */
        __asm__("" : "=r"(vector) : "0"(vector));
        g_battle_map_camera_rotation.vx = vector[0];
        g_battle_map_camera_rotation.vy = vector[1];
        g_battle_map_camera_rotation.vz = vector[2];
        battle_camera_set_focus_and_cursor_tile((battle_screen_coords_t*)&g_battle_camera_integer_coords,
            (battle_camera_rotation_t*)&g_battle_map_camera_rotation);
        break;
    case GNS_RESOURCE_SET_INDOOR:
        WEATHER->ignore_weather = 1;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    case GNS_RESOURCE_SET_OUTDOOR:
        WEATHER->ignore_weather = 0;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    case GNS_RESOURCE_CLEAR_SNOW:
        WEATHER->snow = 0;
        break;
    case GNS_RESOURCE_SET_SNOW:
        WEATHER->snow = 1;
        break;
    case GNS_RESOURCE_SET_WEATHER_MODIFIER:
        WEATHER->modifier = ((s16*)data)[6];
        break;
    }
    return result;
}

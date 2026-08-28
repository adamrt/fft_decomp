#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/libc.h"
#include "psx/types.h"

typedef struct mesh_triangle_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
} mesh_triangle_positions_t;

typedef struct mesh_quad_positions_t {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} mesh_quad_positions_t;

typedef struct mesh_triangle_normals_t {
    s16 x0, y0, z0;
    u16 _pad06;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
} mesh_triangle_normals_t;

typedef struct mesh_quad_normals_t {
    s16 x0, y0, z0;
    u16 _pad06;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} mesh_quad_normals_t;

/* g_battle_map_mesh_parts viewed from vectors_a[1] (0x800fbe08), bound
 * separately: the part loop's strength-reduced pointers depend on that base. */
typedef struct battle_map_mesh_part_view {
    SVECTOR rotation;     /* 0x00; vectors_a[1] */
    u8 _unknown_08[0x10]; /* 0x08 */
    SVECTOR translation;  /* 0x18; vectors_b[1] */
    u8 _unknown_20[0x18]; /* 0x20 */
    VECTOR scale;         /* 0x38; vectors_c[1] */
    u8 _unknown_48[0x10]; /* 0x48 */
    MATRIX matrix;        /* 0x58; matrix_60 */
    u8 _unknown_78[6];    /* 0x78 */
    u8 parent;            /* 0x7e */
    u8 _unknown_7f;       /* 0x7f */
    u16 starts[4];        /* 0x80 */
    u16 counts[4];        /* 0x88 */
    u8 _unknown_90[8];    /* 0x90 */
} battle_map_mesh_part_view_t;

typedef struct battle_weather_splash_rects {
    RECT rects[4];
} battle_weather_splash_rects_t;

extern battle_weather_splash_rects_t g_battle_map_weather_splash_uv_rects;

extern mesh_triangle_positions_t* g_battle_map_part_textured_triangle_positions;
extern mesh_quad_positions_t* g_battle_map_part_textured_quad_positions;
extern mesh_triangle_positions_t* g_battle_map_part_untextured_triangle_positions;
extern mesh_quad_positions_t* g_battle_map_part_untextured_quad_positions;
extern mesh_triangle_normals_t* g_battle_map_part_textured_triangle_normals;
extern mesh_quad_normals_t* g_battle_map_part_textured_quad_normals;
extern mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern mesh_quad_positions_t g_battle_map_textured_quad_positions[];
extern mesh_triangle_positions_t g_battle_map_untextured_triangle_positions[];
extern mesh_quad_positions_t g_battle_map_untextured_quad_positions[];
extern mesh_triangle_normals_t g_battle_map_textured_triangle_normals[];
extern mesh_quad_normals_t g_battle_map_textured_quad_normals[];
extern battle_map_mesh_part_view_t g_battle_map_mesh_parts_view[9];
extern void* g_current_effect_work;
extern s32 g_battle_map_loaded_id;
extern MATRIX g_battle_map_weather_matrix;
extern SVECTOR g_battle_map_weather_rotation;
extern VECTOR g_battle_map_weather_translation;
extern VECTOR g_battle_map_weather_scale;
extern s32 g_battle_map_weather_sprite_scale;
extern SVECTOR g_battle_map_command_0x96_rotation;
extern map_tile_t* g_battle_map_tile_data_ptr;
extern battle_weather_particle_t g_battle_map_weather_particles[64];
extern battle_weather_particle_t g_battle_map_rain_splash_particles[32];
extern u8 g_battle_map_weather_fall_speed;
extern u8 g_battle_map_weather_primary_speed;
extern u8 g_battle_map_rain_secondary_fall_speed;
extern u8 g_battle_map_tile_width;
extern u8 g_battle_map_tile_depth;
extern s32 g_battle_map_lowest_surface_y;
extern battle_render_buffer_t* g_battle_map_ambient_restored_buffer;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_textured_quad_count;
extern u8 g_battle_map_ambient_polygon_color[3];
extern u16 g_battle_target_color_tile_x;
extern u16 g_battle_target_color_tile_y;
extern s16 g_battle_target_tile_mark_modes[3];
extern battle_render_buffer_t* g_battle_target_tile_color_buffer;

extern u8* battle_map_light_state_command(s32 command, u8* data);
extern void battle_map_update_texture_animations(void);
extern void battle_map_update_mesh_part_animations(void);
extern void MulMatrix0(MATRIX* m0, void* m1, MATRIX* m2);
extern void SetLightMatrix(MATRIX* m);
extern void PushMatrix(void);
extern void PopMatrix(void);
extern void MulMatrix2(MATRIX* m0, MATRIX* m1);
extern void TransMatrix(void* m, void* v);
extern void ScaleMatrix(void* m, void* v);
extern void battle_map_queue_textured_triangles(u32* otag, void* prims, s32 depth, s32* count);
extern void battle_map_queue_textured_quads(u32* otag, void* prims, s32 depth, s32* count);
extern void battle_map_queue_untextured_triangles(u32* otag, void* prims, s32 depth, s32* count);
extern void battle_map_queue_untextured_quads(u32* otag, void* prims, s32 depth, s32* count);
/* The -0x20 colour deltas are passed as sign-extended words (0xffffffe0 in
 * the argument registers and stack slots); the u16 prototype would
 * zero-extend them, so this caller uses an all-s32 view.
 * The definition itself only matches with u16 colour parameters (s16 or s32
 * ones change its code), so the two views stay. */
#define MODIFY_PALETTE_WORDS ((void (*)(s32, s32, s32, s32, s32, s32, s32, s32))battle_map_modify_palette)

#define GT3(i)       (g_battle_data->gt3[i])
#define GT4(i)       (g_battle_data->gt4[i])
#define SAVED_GT3(i) (g_battle_target_tile_color_buffer->gt3[i])
#define SAVED_GT4(i) (g_battle_target_tile_color_buffer->gt4[i])

/* Queue the map mesh parts and weather particles for one frame, then apply or
 * restore the targeting and movement tile colours. */
void battle_map_draw_mesh_and_weather(MATRIX* camera) {
    MATRIX light_rotation;
    MATRIX light_matrix;
    MATRIX effect_matrix;
    battle_weather_splash_rects_t splash_uv;
    battle_weather_sprite_rects_t drop_uv;
    VECTOR effect_translation;
    VECTOR unused_vector;
    VECTOR screen;
    CVECTOR unused;
    s32 i;
    long flag;
    long effect_flag;
    s32 part;
    SVECTOR* rotation;
    MATRIX* matrix;
    s32 effect_base;
    s32 j;
    s16 sprite;
    s32 half_size;
    s32 depth;
    u16 sx;
    u16 sy;
    s32 splash_y;
    s16 ground;
    s32 spawn_y;
    s16 tile;
    u16 frame;

    splash_uv = g_battle_map_weather_splash_uv_rects;
    drop_uv = g_battle_map_weather_drop_uv_rects;
    unused = g_battle_map_white_cvector;
    AddPrim(g_battle_data->otag + 0x17F, &g_battle_data->overlay_modes[1]);
    AddPrim(g_battle_data->otag + 0x17F, &g_battle_data->overlay);
    AddPrim(g_battle_data->otag + 0x17F, &g_battle_data->overlay_modes[0]);
    battle_map_update_texture_animations();
    battle_map_update_mesh_part_animations();
    if (g_battle_map_lightning_mode != 0 && g_battle_sound_weather_sfx_enabled != 0) {
        if (rand() % 480 == 0) {
            g_battle_map_lightning_state.timer = 60;
            if (rand() & 1) {
                g_battle_sound_secondary_weather_sfx_id = 0x10003;
                main_sound_play_sfx_find_channel(0x10003);
            } else {
                g_battle_sound_tertiary_weather_sfx_id = 0x1000E;
                main_sound_play_sfx_find_channel(0x1000E);
            }
        }
        if (g_battle_map_lightning_state.timer != 0) {
            u32 remaining;

            remaining = --g_battle_map_lightning_state.timer;
            g_battle_map_lightning_state.colors[0].red = g_battle_map_lightning_state.base_colors[0].red
                - g_battle_map_lightning_state.base_colors[0].red * remaining / 60 + remaining * 208 / 60;
            g_battle_map_lightning_state.colors[0].green = g_battle_map_lightning_state.base_colors[0].green
                - g_battle_map_lightning_state.base_colors[0].green * remaining / 60 + remaining * 208 / 60;
            g_battle_map_lightning_state.colors[0].blue = g_battle_map_lightning_state.base_colors[0].blue
                - g_battle_map_lightning_state.base_colors[0].blue * remaining / 60 + remaining * 3;
            g_battle_map_lightning_state.colors[1].red = g_battle_map_lightning_state.base_colors[1].red
                - g_battle_map_lightning_state.base_colors[1].red * remaining / 60 + remaining * 112 / 60;
            g_battle_map_lightning_state.colors[1].green = g_battle_map_lightning_state.base_colors[1].green
                - g_battle_map_lightning_state.base_colors[1].green * remaining / 60 + remaining * 112 / 60;
            g_battle_map_lightning_state.colors[1].blue = g_battle_map_lightning_state.base_colors[1].blue
                - g_battle_map_lightning_state.base_colors[1].blue * remaining / 60 + remaining * 96 / 60;
            battle_map_light_state_command(
                MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &g_battle_map_lightning_state.colors[0].red);
        } else {
            battle_map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
            if (g_battle_map_lightning_mode == 2) {
                g_battle_map_lightning_mode = 0;
            }
        }
        if (g_battle_map_lightning_state.timer == 0x3A || g_battle_map_lightning_state.timer == 0x38) {
            SetColorMatrix(&g_battle_map_lightning_state.color_matrices[1]);
            SetBackColor(0, 0, 0x40);
        }
        if (g_battle_map_lightning_state.timer == 0x39 || g_battle_map_lightning_state.timer == 0x33) {
            SetColorMatrix(&g_battle_map_lightning_state.color_matrices[0]);
            SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        }
    }
    if (g_battle_map_light_transition_command != 0) {
        battle_map_light_matrix_command(
            g_battle_map_light_transition_command, &g_battle_map_light_color_matrix, &g_battle_map_light_matrix, 0);
    }
    g_battle_data->overlay.r0 = g_battle_map_background_gradient_colors.first.red;
    g_battle_data->overlay.g0 = g_battle_map_background_gradient_colors.first.green;
    g_battle_data->overlay.b0 = g_battle_map_background_gradient_colors.first.blue;
    g_battle_data->overlay.r1 = g_battle_map_background_gradient_colors.first.red;
    g_battle_data->overlay.g1 = g_battle_map_background_gradient_colors.first.green;
    g_battle_data->overlay.b1 = g_battle_map_background_gradient_colors.first.blue;
    g_battle_data->overlay.r2 = g_battle_map_background_gradient_colors.second.red;
    g_battle_data->overlay.g2 = g_battle_map_background_gradient_colors.second.green;
    g_battle_data->overlay.b2 = g_battle_map_background_gradient_colors.second.blue;
    g_battle_data->overlay.r3 = g_battle_map_background_gradient_colors.second.red;
    g_battle_data->overlay.g3 = g_battle_map_background_gradient_colors.second.green;
    g_battle_data->overlay.b3 = g_battle_map_background_gradient_colors.second.blue;
    MulMatrix0(&g_battle_map_light_matrix, &g_battle_map_light_rotation_matrix, &g_battle_map_local_light_matrix);
    SetLightMatrix(&g_battle_map_local_light_matrix);
    if (g_battle_map_polygon_flag_clear_countdown != 0) {
        if (--g_battle_map_polygon_flag_clear_countdown == 0) {
            battle_map_polygon_flag_command(0x47);
        }
    }
    SetRotMatrix(camera);
    SetTransMatrix(camera);
    g_battle_map_part_textured_triangle_count = g_battle_map_mesh_parts_view[0].counts[0];
    g_battle_map_part_textured_triangle_positions
        = &g_battle_map_textured_triangle_positions[g_battle_map_mesh_parts_view[0].starts[0]];
    g_battle_map_part_textured_triangle_normals
        = &g_battle_map_textured_triangle_normals[g_battle_map_mesh_parts_view[0].starts[0]];
    battle_map_queue_textured_triangles(g_battle_data->otag,
        &g_battle_data->gt3[g_battle_map_mesh_parts_view[0].starts[0]], g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts_view[0].counts[1];
    g_battle_map_part_textured_quad_positions
        = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts_view[0].starts[1]];
    g_battle_map_part_textured_quad_normals
        = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts_view[0].starts[1]];
    battle_map_queue_textured_quads(g_battle_data->otag, &g_battle_data->gt4[g_battle_map_mesh_parts_view[0].starts[1]],
        g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts_view[0].counts[2];
    g_battle_map_part_untextured_triangle_positions
        = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts_view[0].starts[2]];
    battle_map_queue_untextured_triangles(g_battle_data->otag,
        &g_battle_data->f3[g_battle_map_mesh_parts_view[0].starts[2]], g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts_view[0].counts[3];
    g_battle_map_part_untextured_quad_positions
        = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts_view[0].starts[3]];
    battle_map_queue_untextured_quads(g_battle_data->otag,
        &g_battle_data->f4[g_battle_map_mesh_parts_view[0].starts[3]], g_battle_map_camera_rotation.vy, &i);
    /* Dead store: `matrix` is reassigned for every drawn child part before
     * use. It still survives the first CSE pass (the loop uses `matrix`), and
     * the second pass reuses it for the loop-hoisted &g_battle_map_mesh_parts_view[0].matrix
     * constant; reload then deletes that spilled constant together with this
     * set. The transient local moves the draw calls' `&i` temporary from v0
     * to a0, as in the target. Earlier than the counts[0] access, it would
     * become the base of that address instead. */
    matrix = &g_battle_map_mesh_parts_view[0].matrix;
    PushMatrix();
    for (part = 1; part < 9; part++) {
        if (g_battle_map_mesh_animation_instructions[part].states[0][1].next != 0) {
            u8 parent;

            rotation = &g_battle_map_mesh_parts_view[part].rotation;
            matrix = &g_battle_map_mesh_parts_view[part].matrix;
            parent = g_battle_map_mesh_parts_view[part].parent;
            RotMatrix(rotation, matrix);
            MulMatrix2(&g_battle_map_mesh_parts_view[parent].matrix, matrix);
            SetRotMatrix(&g_battle_map_mesh_parts_view[parent].matrix);
            SetTransMatrix(&g_battle_map_mesh_parts_view[parent].matrix);
            RotTrans(&g_battle_map_mesh_parts_view[part].translation, (VECTOR*)matrix->t, &flag);
            ScaleMatrix(matrix, &g_battle_map_mesh_parts_view[part].scale);
            RotMatrix(rotation, &light_rotation);
            MulMatrix0(&g_battle_map_local_light_matrix, &light_rotation, &light_matrix);
            SetLightMatrix(&light_matrix);
            SetRotMatrix(matrix);
            SetTransMatrix(matrix);
            g_battle_map_part_textured_triangle_count = g_battle_map_mesh_parts_view[part].counts[0];
            g_battle_map_part_textured_triangle_positions
                = &g_battle_map_textured_triangle_positions[g_battle_map_mesh_parts_view[part].starts[0]];
            g_battle_map_part_textured_triangle_normals
                = &g_battle_map_textured_triangle_normals[g_battle_map_mesh_parts_view[part].starts[0]];
            battle_map_queue_textured_triangles(g_battle_data->otag,
                &g_battle_data->gt3[g_battle_map_mesh_parts_view[part].starts[0]], g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts_view[part].counts[1];
            g_battle_map_part_textured_quad_positions
                = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts_view[part].starts[1]];
            g_battle_map_part_textured_quad_normals
                = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts_view[part].starts[1]];
            battle_map_queue_textured_quads(g_battle_data->otag,
                &g_battle_data->gt4[g_battle_map_mesh_parts_view[part].starts[1]], g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts_view[part].counts[2];
            g_battle_map_part_untextured_triangle_positions
                = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts_view[part].starts[2]];
            battle_map_queue_untextured_triangles(g_battle_data->otag,
                &g_battle_data->f3[g_battle_map_mesh_parts_view[part].starts[2]], g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts_view[part].counts[3];
            g_battle_map_part_untextured_quad_positions
                = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts_view[part].starts[3]];
            battle_map_queue_untextured_quads(g_battle_data->otag,
                &g_battle_data->f4[g_battle_map_mesh_parts_view[part].starts[3]], g_battle_map_camera_rotation.vy, &i);
            if (g_battle_map_untextured_otz_mask != 0 && g_current_effect_work != 0
                && (g_battle_map_loaded_id == 0x35 || g_battle_map_loaded_id == 0x37
                    || g_battle_map_loaded_id == 0x53)) {
                RotMatrix(&g_battle_map_mesh_parts_view[part].rotation, &effect_matrix);
                effect_translation.vx = g_battle_map_mesh_parts_view[part].translation.vx;
                effect_translation.vy = g_battle_map_mesh_parts_view[part].translation.vy;
                effect_translation.vz = g_battle_map_mesh_parts_view[part].translation.vz;
                TransMatrix(&effect_matrix, &effect_translation);
                ScaleMatrix(&effect_matrix, &g_battle_map_mesh_parts_view[part].scale);
                SetRotMatrix(&effect_matrix);
                SetTransMatrix(&effect_matrix);
                for (j = 0; j < 4; j++) {
                    effect_base = (part - 1) * 4 + 0x61;
                    RotTrans(&g_battle_map_mesh_animation_keyframes[j + effect_base].vector_8,
                        (VECTOR*)g_current_effect_work + j + (part - 1) * 4, &effect_flag);
                }
            }
        }
    }
    PopMatrix();
    if (g_battle_map_weather_effect_mode == 0x53) {
        PushMatrix();
        g_battle_map_weather_rotation.vy += g_battle_map_weather_primary_speed;
        RotMatrix(&g_battle_map_weather_rotation, &g_battle_map_weather_matrix);
        TransMatrix(&g_battle_map_weather_matrix, &g_battle_map_weather_translation);
        ScaleMatrix(&g_battle_map_weather_matrix, &g_battle_map_weather_scale);
        SetRotMatrix(&g_battle_map_weather_matrix);
        SetTransMatrix(&g_battle_map_weather_matrix);
        half_size = g_battle_map_camera_zoom.vx * g_battle_map_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (s16)(g_battle_map_weather_particles[i].z / 28) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / 28;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height >> 5))
                * 12);
            if (g_battle_render_buffers[0].ft3[i].y0 >= 0x119) {
                if (g_battle_map_weather_scale.vy != 0) {
                    g_battle_map_weather_particles[i].y = (s32)0xFFF00000 / g_battle_map_weather_scale.vy;
                }
                g_battle_map_weather_particles[i].fall_speed = 0;
                sprite = rand() % 3;

                g_battle_render_buffers[0].ft3[i].u0 = drop_uv.rects[sprite].x;
                g_battle_render_buffers[1].ft3[i].u0 = drop_uv.rects[sprite].x;
                g_battle_render_buffers[0].ft3[i].u1 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[1].ft3[i].u1 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[0].ft3[i].u2 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[1].ft3[i].u2 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[0].ft3[i].v0 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[1].ft3[i].v0 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[0].ft3[i].v1 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[1].ft3[i].v1 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[0].ft3[i].v2 = drop_uv.rects[sprite].y + drop_uv.rects[sprite].h;
                g_battle_render_buffers[1].ft3[i].v2 = drop_uv.rects[sprite].y + drop_uv.rects[sprite].h;
                g_battle_map_weather_particles[i].fall_speed = sprite + g_battle_map_weather_fall_speed;
                if (g_battle_map_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_battle_map_weather_particles[i].x = rand() % (0x8C000 / g_battle_map_weather_scale.vx);
                    } else {
                        g_battle_map_weather_particles[i].x = -(rand() % (0x8C000 / g_battle_map_weather_scale.vx));
                    }
                }
                if (g_battle_map_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_battle_map_weather_particles[i].z = rand() % (0x8C000 / g_battle_map_weather_scale.vz);
                    } else {
                        g_battle_map_weather_particles[i].z = -(rand() % (0x8C000 / g_battle_map_weather_scale.vz));
                    }
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_battle_map_weather_particles[i].x = g_battle_map_weather_particles[i].x + 1;
                } else {
                    g_battle_map_weather_particles[i].x = g_battle_map_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_battle_map_weather_particles[i].z = g_battle_map_weather_particles[i].z + 1;
                } else {
                    g_battle_map_weather_particles[i].z = g_battle_map_weather_particles[i].z - 1;
                }
            }
            g_battle_map_weather_particles[i].y += g_battle_map_weather_particles[i].fall_speed * g_animation_speed;
            RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_battle_data->ft3[i].x1 = sx + half_size;
                g_battle_data->ft3[i].x2 = sx + half_size;
                g_battle_data->ft3[i].y2 = sy + half_size;
                g_battle_data->ft3[i].x0 = sx;
                g_battle_data->ft3[i].y0 = sy;
                g_battle_data->ft3[i].y1 = sy;
                AddPrim(g_battle_data->otag + 2, &g_battle_data->ft3[i]);
            }
        }
        PopMatrix();
    }
    if (g_battle_map_weather_effect_mode == 0x96) {
        s32 sine;

        PushMatrix();
        g_battle_map_command_0x96_rotation.vy = g_battle_map_camera_rotation.vy;
        g_battle_map_command_0x96_rotation.vz
            = -(rsin((g_battle_map_command_0x96_frame << 10) / g_battle_map_command_0x96_duration) >> 4);
        sine = rsin((g_battle_map_command_0x96_frame << 10) / g_battle_map_command_0x96_duration);
        g_battle_map_weather_scale.vx = sine * 4 + ONE;
        g_battle_map_weather_scale.vz = sine * 4 + ONE;
        g_battle_map_weather_scale.vy = sine + ONE;
        g_battle_map_command_0x96_frame++;
        RotMatrix(&g_battle_map_command_0x96_rotation, &g_battle_map_weather_matrix);
        TransMatrix(&g_battle_map_weather_matrix, &g_battle_map_weather_translation);
        ScaleMatrix(&g_battle_map_weather_matrix, &g_battle_map_weather_scale);
        SetRotMatrix(&g_battle_map_weather_matrix);
        SetTransMatrix(&g_battle_map_weather_matrix);
        half_size = g_battle_map_camera_zoom.vx * g_battle_map_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (s16)(g_battle_map_weather_particles[i].z / 28) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / 28;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height >> 5))
                * 12);
            tile = (s16)(g_battle_map_weather_particles[i + 32].z / 28) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i + 32].x / 28;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height >> 5))
                * 12);
            if ((g_battle_map_command_0x96_frame < g_battle_map_command_0x96_duration / 8
                    && g_battle_render_buffers[0].ft3[i].y0 >= 0x119)
                || (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame
                    && g_battle_render_buffers[0].ft3[i].y0 < 0)) {
                if (g_battle_map_weather_scale.vy != 0) {
                    if (g_battle_map_command_0x96_frame < g_battle_map_command_0x96_duration / 8) {
                        g_battle_map_weather_particles[i].y = (s32)0xFFF00000 / g_battle_map_weather_scale.vy;
                    }
                    if (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame) {
                        g_battle_map_weather_particles[i].y = 0x30000 / g_battle_map_weather_scale.vy;
                    }
                }
                sprite = rand() % 3;
                g_battle_map_weather_particles[i].fall_speed = g_battle_map_weather_fall_speed + 3 - sprite;

                g_battle_render_buffers[0].ft3[i].u0 = drop_uv.rects[sprite].x;
                g_battle_render_buffers[1].ft3[i].u0 = drop_uv.rects[sprite].x;
                g_battle_render_buffers[0].ft3[i].u1 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[1].ft3[i].u1 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[0].ft3[i].u2 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[1].ft3[i].u2 = drop_uv.rects[sprite].x + drop_uv.rects[sprite].w;
                g_battle_render_buffers[0].ft3[i].v0 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[1].ft3[i].v0 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[0].ft3[i].v1 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[1].ft3[i].v1 = drop_uv.rects[sprite].y;
                g_battle_render_buffers[0].ft3[i].v2 = drop_uv.rects[sprite].y + drop_uv.rects[sprite].h;
                g_battle_render_buffers[1].ft3[i].v2 = drop_uv.rects[sprite].y + drop_uv.rects[sprite].h;
                if (g_battle_map_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_battle_map_weather_particles[i].x = rand() % (0x8C000 / g_battle_map_weather_scale.vx);
                    } else {
                        g_battle_map_weather_particles[i].x = -(rand() % (0x8C000 / g_battle_map_weather_scale.vx));
                    }
                }
                if (g_battle_map_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_battle_map_weather_particles[i].z = rand() % (0x8C000 / g_battle_map_weather_scale.vz);
                    } else {
                        g_battle_map_weather_particles[i].z = -(rand() % (0x8C000 / g_battle_map_weather_scale.vz));
                    }
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_battle_map_weather_particles[i].x = g_battle_map_weather_particles[i].x + 1;
                } else {
                    g_battle_map_weather_particles[i].x = g_battle_map_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_battle_map_weather_particles[i].z = g_battle_map_weather_particles[i].z + 1;
                } else {
                    g_battle_map_weather_particles[i].z = g_battle_map_weather_particles[i].z - 1;
                }
            }
            if (g_battle_map_command_0x96_frame < g_battle_map_command_0x96_duration / 8) {
                g_battle_map_weather_particles[i].y += g_battle_map_weather_particles[i].fall_speed * g_animation_speed
                    * rsin((g_battle_map_command_0x96_frame << 13) / g_battle_map_command_0x96_duration + 0x400) / 2048;
            }
            if (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame) {
                g_battle_map_weather_particles[i].y -= g_battle_map_weather_particles[i].fall_speed * g_animation_speed;
            }
            RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_battle_data->ft3[i].x1 = sx + half_size;
                g_battle_data->ft3[i].x2 = sx + half_size;
                g_battle_data->ft3[i].y2 = sy + half_size;
                g_battle_data->ft3[i].x0 = sx;
                g_battle_data->ft3[i].y0 = sy;
                g_battle_data->ft3[i].y1 = sy;
                AddPrim(g_battle_data->otag + 2, &g_battle_data->ft3[i]);
            }
        }
        if (g_battle_map_command_0x96_frame == g_battle_map_command_0x96_duration) {
            battle_map_set_weather_texture_overlay(0x8B);
            g_battle_map_weather_scale.vx = ONE;
            g_battle_map_weather_scale.vy = ONE;
            g_battle_map_weather_scale.vz = ONE;
            g_battle_map_weather_rotation.vx = 0x10;
            g_battle_map_weather_rotation.vz = 0x10;
            g_battle_map_weather_translation.vx = 0x100;
            g_battle_map_weather_translation.vy = 0xF0;
            g_battle_map_weather_rotation.vy = 0;
            g_battle_map_weather_translation.vz = 0x280;
        }
        PopMatrix();
    }
    if (g_battle_map_weather_effect_mode == 0x55) {
        half_size = g_battle_map_camera_zoom.vx * 22;
        half_size /= 0x2000;
        for (i = 0; i < 32; i++) {
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (s16)(g_battle_map_weather_particles[i].z / 28) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / 28;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height >> 5))
                * 12);
            if (ground < g_battle_map_weather_particles[i + 32].y) {
                g_battle_map_weather_particles[i].fall_speed = 0;
                g_battle_map_weather_particles[i].x = rand() % (g_battle_map_tile_width * 28);
                g_battle_map_weather_particles[i].z = rand() % (g_battle_map_tile_depth * 28);
                spawn_y = -0x180;
                g_battle_map_weather_particles[i].y = spawn_y;
                g_battle_map_weather_particles[i + 32].x = g_battle_map_weather_particles[i].x;
                g_battle_map_weather_particles[i + 32].z = g_battle_map_weather_particles[i].z;
                g_battle_map_weather_particles[i + 32].y
                    = spawn_y - g_battle_map_weather_fall_speed - rand() % (g_battle_map_weather_primary_speed * 12);
            }
            g_battle_map_weather_particles[i].y += g_battle_map_weather_fall_speed * g_animation_speed;
            g_battle_map_weather_particles[i + 32].y += g_battle_map_weather_fall_speed * g_animation_speed;
            if (i < 16) {
                g_battle_map_weather_particles[i].y += g_battle_map_weather_primary_speed * g_animation_speed;
                g_battle_map_weather_particles[i + 32].y += g_battle_map_weather_primary_speed * g_animation_speed;
            } else {
                g_battle_map_weather_particles[i].y += g_battle_map_rain_secondary_fall_speed * g_animation_speed;
                g_battle_map_weather_particles[i + 32].y += g_battle_map_rain_secondary_fall_speed * g_animation_speed;
            }
            if (g_battle_map_weather_particles[i].y >= -0x17F) {
                if (ground < g_battle_map_weather_particles[i].y) {
                    g_battle_map_weather_particles[i].y = g_battle_map_lowest_surface_y;
                } else if (g_battle_map_weather_particles[i + 32].y < ground) {
                    RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
                    sx = screen.vx;
                    sy = screen.vy;
                    g_battle_data->ft3[i].x2 = sx + 1;
                    g_battle_data->ft3[i].x0 = sx;
                    g_battle_data->ft3[i].y0 = sy;
                    g_battle_data->ft3[i].y2 = sy - 1;
                    RotTrans((SVECTOR*)&g_battle_map_weather_particles[i + 32], &screen, &flag);
                    depth = screen.vz / 4;
                    if (depth > 0 && depth < 0x180) {
                        sx = screen.vx;
                        sy = screen.vy;
                        g_battle_data->ft3[i].x1 = sx;
                        g_battle_data->ft3[i].y1 = sy;
                        AddPrim(g_battle_data->otag + depth, &g_battle_data->ft3[i]);
                    }
                }
            }
            if (g_battle_map_weather_particles[i].fall_speed == 0
                && ((ground < g_battle_map_weather_particles[i].y && g_battle_map_weather_particles[i + 32].y < ground)
                    || g_battle_map_weather_particles[i].y == g_battle_map_lowest_surface_y)) {
                g_battle_map_rain_splash_particles[i].fall_speed = 0x10;
                g_battle_map_rain_splash_particles[i].y = ground - 8;
                g_battle_map_rain_splash_particles[i].x = g_battle_map_weather_particles[i].x;
                g_battle_map_rain_splash_particles[i].z = g_battle_map_weather_particles[i].z;
                if (!(g_battle_map_tile_data_ptr[tile].flags_06.value & 2)) {
                    g_battle_map_weather_particles[i].fall_speed = 1;
                } else {
                    g_battle_map_weather_particles[i].fall_speed = 2;
                }
            }
            if (g_battle_map_rain_splash_particles[i].fall_speed != 0) {
                RotTrans((SVECTOR*)&g_battle_map_rain_splash_particles[i], &screen, &flag);
                depth = screen.vz / 4;
                if (depth > 0 && depth < 0x180) {
                    sx = screen.vx;
                    sy = screen.vy;
                    splash_y = sy - 4;
                    g_battle_data->ft3[i + 32].x0 = sx - half_size;
                    g_battle_data->ft3[i + 32].y0 = splash_y - half_size;
                    g_battle_data->ft3[i + 32].x1 = sx + half_size;
                    g_battle_data->ft3[i + 32].y1 = splash_y + half_size;
                    g_battle_data->ft3[i + 32].x2 = sx - half_size;
                    g_battle_data->ft3[i + 32].y2 = splash_y + half_size;
                    if (g_battle_map_weather_particles[i].fall_speed == 1) {
                        AddPrim(g_battle_data->otag + depth, &g_battle_data->ft3[i + 32]);
                    }
                }
                g_battle_map_rain_splash_particles[i].fall_speed--;
                frame = g_battle_map_rain_splash_particles[i].fall_speed;
                if ((frame & 3) == 0) {
                    g_battle_render_buffers[0].ft3[i + 32].u0 = splash_uv.rects[(frame >> 2) & 3].x;
                    g_battle_render_buffers[1].ft3[i + 32].u0 = splash_uv.rects[(frame >> 2) & 3].x;
                    g_battle_render_buffers[0].ft3[i + 32].u1
                        = splash_uv.rects[(frame >> 2) & 3].x + splash_uv.rects[(frame >> 2) & 3].w;
                    g_battle_render_buffers[1].ft3[i + 32].u1
                        = splash_uv.rects[(frame >> 2) & 3].x + splash_uv.rects[(frame >> 2) & 3].w;
                    g_battle_render_buffers[0].ft3[i + 32].u2 = splash_uv.rects[(frame >> 2) & 3].x;
                    g_battle_render_buffers[1].ft3[i + 32].u2 = splash_uv.rects[(frame >> 2) & 3].x;
                    g_battle_render_buffers[0].ft3[i + 32].v0 = splash_uv.rects[(frame >> 2) & 3].y;
                    g_battle_render_buffers[1].ft3[i + 32].v0 = splash_uv.rects[(frame >> 2) & 3].y;
                    g_battle_render_buffers[0].ft3[i + 32].v1
                        = splash_uv.rects[(frame >> 2) & 3].y + splash_uv.rects[(frame >> 2) & 3].h;
                    g_battle_render_buffers[1].ft3[i + 32].v1
                        = splash_uv.rects[(frame >> 2) & 3].y + splash_uv.rects[(frame >> 2) & 3].h;
                    g_battle_render_buffers[0].ft3[i + 32].v2
                        = splash_uv.rects[(frame >> 2) & 3].y + splash_uv.rects[(frame >> 2) & 3].h;
                    g_battle_render_buffers[1].ft3[i + 32].v2
                        = splash_uv.rects[(frame >> 2) & 3].y + splash_uv.rects[(frame >> 2) & 3].h;
                }
                if (g_battle_map_rain_splash_particles[i].fall_speed == 0) {
                    g_battle_map_weather_particles[i].fall_speed = 0;
                }
            }
        }
    }
    /* The ambient colour restores use pointer arithmetic on the decayed arrays
     * rather than GT3(i)/GT4(i) indexing: that keeps the target's
     * `addu offset, base` operand order (as in battle_target_set_tile_background_color). */
    if (g_battle_map_ambient_restored_buffer != 0) {
        if (g_battle_map_ambient_restored_buffer != g_battle_data) {
            for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
                if (!((s16)g_battle_map_textured_triangle_positions[i]._pad0e & 0x8000)) {
                    (g_battle_data->gt3 + i)->r0 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt3 + i)->g0 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt3 + i)->b0 = g_battle_map_ambient_polygon_color[2];
                    (g_battle_data->gt3 + i)->r1 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt3 + i)->g1 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt3 + i)->b1 = g_battle_map_ambient_polygon_color[2];
                    (g_battle_data->gt3 + i)->r2 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt3 + i)->g2 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt3 + i)->b2 = g_battle_map_ambient_polygon_color[2];
                }
            }
            for (i = 0; i < g_battle_map_textured_quad_count; i++) {
                if (!((s16)g_battle_map_textured_quad_positions[i]._pad0e & 0x8000)) {
                    (g_battle_data->gt4 + i)->r0 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt4 + i)->g0 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt4 + i)->b0 = g_battle_map_ambient_polygon_color[2];
                    (g_battle_data->gt4 + i)->r1 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt4 + i)->g1 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt4 + i)->b1 = g_battle_map_ambient_polygon_color[2];
                    (g_battle_data->gt4 + i)->r2 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt4 + i)->g2 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt4 + i)->b2 = g_battle_map_ambient_polygon_color[2];
                    (g_battle_data->gt4 + i)->r3 = g_battle_map_ambient_polygon_color[0];
                    (g_battle_data->gt4 + i)->g3 = g_battle_map_ambient_polygon_color[1];
                    (g_battle_data->gt4 + i)->b3 = g_battle_map_ambient_polygon_color[2];
                }
            }
        }
        g_battle_map_ambient_restored_buffer = 0;
    }
    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    if (g_battle_target_tile_mark_modes[0] == 1) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | 0x7840;
                    GT3(i).r0 = 0x20;
                    GT3(i).r1 = 0x20;
                    GT3(i).r2 = 0x20;
                    GT3(i).g0 = 0x28;
                    GT3(i).g1 = 0x28;
                    GT3(i).g2 = 0x28;
                    GT3(i).b0 = 0x60;
                    GT3(i).b1 = 0x60;
                    GT3(i).b2 = 0x60;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x20) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | 0x7840;
                    GT4(i).r0 = 0x20;
                    GT4(i).r1 = 0x20;
                    GT4(i).r2 = 0x20;
                    GT4(i).r3 = 0x20;
                    GT4(i).g0 = 0x28;
                    GT4(i).g1 = 0x28;
                    GT4(i).g2 = 0x28;
                    GT4(i).g3 = 0x28;
                    GT4(i).b0 = 0x60;
                    GT4(i).b1 = 0x60;
                    GT4(i).b2 = 0x60;
                    GT4(i).b3 = 0x60;
                }
            }
        }
        g_battle_target_tile_mark_modes[0] = 10;
    }
    if (g_battle_target_tile_mark_modes[1] == 2) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | 0x7840;
                    GT3(i).r0 = 0x60;
                    GT3(i).r1 = 0x60;
                    GT3(i).r2 = 0x60;
                    GT3(i).g0 = 0x18;
                    GT3(i).g1 = 0x18;
                    GT3(i).g2 = 0x18;
                    GT3(i).b0 = 0x20;
                    GT3(i).b1 = 0x20;
                    GT3(i).b2 = 0x20;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x40) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | 0x7840;
                    GT4(i).r0 = 0x60;
                    GT4(i).r1 = 0x60;
                    GT4(i).r2 = 0x60;
                    GT4(i).r3 = 0x60;
                    GT4(i).g0 = 0x18;
                    GT4(i).g1 = 0x18;
                    GT4(i).g2 = 0x18;
                    GT4(i).g3 = 0x18;
                    GT4(i).b0 = 0x20;
                    GT4(i).b1 = 0x20;
                    GT4(i).b2 = 0x20;
                    GT4(i).b3 = 0x20;
                }
            }
        }
        g_battle_target_tile_mark_modes[1] = 11;
    }
    if (g_battle_target_tile_mark_modes[2] == 7) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x80) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | 0x7840;
                    GT3(i).r0 = 0x50;
                    GT3(i).r1 = 0x50;
                    GT3(i).r2 = 0x50;
                    GT3(i).g0 = 0x60;
                    GT3(i).g1 = 0x60;
                    GT3(i).g2 = 0x60;
                    GT3(i).b0 = 0x10;
                    GT3(i).b1 = 0x10;
                    GT3(i).b2 = 0x10;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != 0xFFFE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = (s16)g_battle_target_color_tile_y * g_battle_map_tile_width + (s16)g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks & 0x80) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | 0x7840;
                    GT4(i).r0 = 0x50;
                    GT4(i).r1 = 0x50;
                    GT4(i).r2 = 0x50;
                    GT4(i).r3 = 0x50;
                    GT4(i).g0 = 0x60;
                    GT4(i).g1 = 0x60;
                    GT4(i).g2 = 0x60;
                    GT4(i).g3 = 0x60;
                    GT4(i).b0 = 0x10;
                    GT4(i).b1 = 0x10;
                    GT4(i).b2 = 0x10;
                    GT4(i).b3 = 0x10;
                }
            }
        }
        g_battle_target_tile_mark_modes[2] = 12;
    }
    if (g_battle_target_tile_mark_modes[0] == 5) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
            if (!((s16)g_battle_map_textured_triangle_positions[i]._pad0e & 0x8000)) {
                GT3(i).r0 = SAVED_GT3(i).r0;
                GT3(i).r1 = SAVED_GT3(i).r1;
                GT3(i).r2 = SAVED_GT3(i).r2;
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
            if (!((s16)g_battle_map_textured_quad_positions[i]._pad0e & 0x8000)) {
                GT4(i).r0 = SAVED_GT4(i).r0;
                GT4(i).r1 = SAVED_GT4(i).r1;
                GT4(i).r2 = SAVED_GT4(i).r2;
                GT4(i).r3 = SAVED_GT4(i).r3;
            }
        }
        g_battle_target_tile_mark_modes[0] = 0;
        if ((g_battle_target_tile_mark_modes[1] | g_battle_target_tile_mark_modes[2]) == 0) {
            MODIFY_PALETTE_WORDS(10, 8, 1, 0, 1, -0x20, -0x20, -0x20);
            MODIFY_PALETTE_WORDS(8, 8, 1, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_battle_target_tile_mark_modes[1] == 3) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
        }
        g_battle_target_tile_mark_modes[1] = 0;
        if ((g_battle_target_tile_mark_modes[0] | g_battle_target_tile_mark_modes[2]) == 0) {
            MODIFY_PALETTE_WORDS(10, 8, 1, 0, 1, -0x20, -0x20, -0x20);
            MODIFY_PALETTE_WORDS(8, 8, 1, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_battle_target_tile_mark_modes[2] == 8) {
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[0]; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
            GT3(i).r0 = SAVED_GT3(i).r0;
            GT3(i).r1 = SAVED_GT3(i).r1;
            GT3(i).r2 = SAVED_GT3(i).r2;
            GT3(i).g0 = SAVED_GT3(i).g0;
            GT3(i).g1 = SAVED_GT3(i).g1;
            GT3(i).g2 = SAVED_GT3(i).g2;
            GT3(i).b0 = SAVED_GT3(i).b0;
            GT3(i).b1 = SAVED_GT3(i).b1;
            GT3(i).b2 = SAVED_GT3(i).b2;
        }
        for (i = 0; i < g_battle_map_mesh_parts_view[0].counts[1]; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
            GT4(i).r0 = SAVED_GT4(i).r0;
            GT4(i).r1 = SAVED_GT4(i).r1;
            GT4(i).r2 = SAVED_GT4(i).r2;
            GT4(i).r3 = SAVED_GT4(i).r3;
            GT4(i).g0 = SAVED_GT4(i).g0;
            GT4(i).g1 = SAVED_GT4(i).g1;
            GT4(i).g2 = SAVED_GT4(i).g2;
            GT4(i).g3 = SAVED_GT4(i).g3;
            GT4(i).b0 = SAVED_GT4(i).b0;
            GT4(i).b1 = SAVED_GT4(i).b1;
            GT4(i).b2 = SAVED_GT4(i).b2;
            GT4(i).b3 = SAVED_GT4(i).b3;
        }
        g_battle_target_tile_mark_modes[2] = 0;
        if ((g_battle_target_tile_mark_modes[0] | g_battle_target_tile_mark_modes[1]) == 0) {
            MODIFY_PALETTE_WORDS(10, 8, 1, 0, 1, -0x20, -0x20, -0x20);
            MODIFY_PALETTE_WORDS(8, 8, 1, 0, 1, -0x20, -0x20, -0x20);
        }
    }
}

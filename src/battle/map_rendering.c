/* Map lighting, tile background colors, mesh and weather rendering, texture animations and GNS resource dispatch, with
 * the weather and vector tables they use. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void battle_map_init_weather_particles(map_weather_command_e command);
static void battle_map_update_mesh_part_animations(void);
static void battle_map_update_texture_animations(void);

static void battle_map_polygon_flag_command(s32 command) {
    s32 i;

    switch (command) {
    case MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (!(g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
            }
        }
        g_battle_map_ambient_restored_buffer = g_battle_gfx_current_render_buffer;
        break;
    case MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_battle_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_battle_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_battle_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_battle_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        break;
    case MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            g_battle_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            g_battle_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        g_battle_map_polygon_flag_clear_countdown = 0;
        break;
    }
}

/* Main-executable libgte routine at 0x8001d108; it loads the matrix into GTE
 * control registers 16-20 (the color matrix), like Psy-Q SetColorMatrix. */

/*
 * Copy two matrices into the working pair at g_battle_map_light_color_matrix and into entries 0 and
 * 2 of g_battle_map_lightning_state.color_matrices, then load the first into the GTE color matrix.
 *
 * Each backup copy rereads the working field; a chained assignment assigns the
 * loop pointers to different registers.
 */
static void battle_map_set_light_matrices(MATRIX* first, MATRIX* second) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_color_matrix.m[i][j] = first->m[i][j];
            g_battle_map_lightning_state.color_matrices[0].m[i][j] = g_battle_map_light_color_matrix.m[i][j];
        }
        g_battle_map_light_color_matrix.t[i] = first->t[i];
        g_battle_map_lightning_state.color_matrices[0].t[i] = g_battle_map_light_color_matrix.t[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_matrix.m[i][j] = second->m[i][j];
            g_battle_map_lightning_state.color_matrices[2].m[i][j] = g_battle_map_light_matrix.m[i][j];
        }
        g_battle_map_light_matrix.t[i] = second->t[i];
        g_battle_map_lightning_state.color_matrices[2].t[i] = g_battle_map_light_matrix.t[i];
    }
    SetColorMatrix(&g_battle_map_light_color_matrix);
}

/* GTE back color commands for the map's ambient color.
 *
 * SET stores the three components and loads them into the GTE, and returns the
 * address of the stored 32-bit red component; while the back color is disabled it does
 * neither and returns 0. GET returns that address. DISABLE loads black and blocks
 * SET; RESTORE reloads the stored color and lifts the block.
 *
 * DISABLE, RESTORE and a command with no case return the unset `result`. Every
 * caller issues SET and ignores the result. */
s32* battle_map_control_gte_background_color(s32 command, s32 value_1, s32 value_2, s32 value_3) {
    /* Left unset on the DISABLE, RESTORE and default paths. */
    s32* result;

    switch (command) {
    case BATTLE_BACKGROUND_COLOR_SET: {
        s32* stored_values;

        if (g_battle_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_battle_map_back_color_red;
        *stored_values = value_1;
        g_battle_map_back_color_green = value_2;
        g_battle_map_back_color_blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case BATTLE_BACKGROUND_COLOR_GET:
        result = &g_battle_map_back_color_red;
        break;
    case BATTLE_BACKGROUND_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_battle_map_back_color_disabled = 1;
        return result;
    case BATTLE_BACKGROUND_COLOR_RESTORE:
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        g_battle_map_back_color_disabled = 0;
        return result;
    }
    return result;
}

/*
 * Map light matrix commands, selected on command - 0x5e: 0x5e loads both
 * matrices from the arguments and applies the color matrix to the GTE, 0x5f
 * saves and 0x60 restores them, 0x61 and 0x62 return the color and light
 * matrices. 0x66 and 0x68 start a transition from the current matrices over
 * duration frames and queue 0x67 or 0x69 in g_battle_map_light_transition_command, which the map draw
 * loop then issues once per frame: 0x67 interpolates toward target matrices,
 * 0x69 adds a frame-scaled delta.
 *
 * The translation rows add the start matrix element m[i][3] (the loop index
 * after the inner loop) rather than the start translation, as in the target.
 * Commands 0x5e-0x60 and 0x66-0x69 return the uninitialized result, like
 * battle_map_light_state_command.
 */
MATRIX* battle_map_light_matrix_command(
    map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration) {
    MATRIX* result;
    s32 i;
    s32 j;

    switch (command) {
    case MAP_LIGHT_MATRIX_COMMAND_SET:
        g_battle_map_light_color_matrix = *color;
        g_battle_map_light_matrix = *light;
        SetColorMatrix(&g_battle_map_light_color_matrix);
        break;
    case MAP_LIGHT_MATRIX_COMMAND_SAVE:
        g_battle_map_saved_light_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_saved_light_matrix = g_battle_map_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_RESTORE:
        g_battle_map_light_color_matrix = g_battle_map_saved_light_color_matrix;
        g_battle_map_light_matrix = g_battle_map_saved_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_GET_COLOR:
        result = &g_battle_map_light_color_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT:
        result = &g_battle_map_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_START_BLEND:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_battle_map_light_color_matrix.m[i][j] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                    + (g_battle_map_light_transition_target_color_matrix.m[i][j]
                          - g_battle_map_light_transition_start_color_matrix.m[i][j])
                        * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
                g_battle_map_light_matrix.m[i][j] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                    + (g_battle_map_light_transition_target_light_matrix.m[i][j]
                          - g_battle_map_light_transition_start_light_matrix.m[i][j])
                        * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
            }
            g_battle_map_light_color_matrix.t[i] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                + (g_battle_map_light_transition_target_color_matrix.t[i]
                      - g_battle_map_light_transition_start_color_matrix.t[i])
                    * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
            g_battle_map_light_matrix.t[i] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                + (g_battle_map_light_transition_target_light_matrix.t[i]
                      - g_battle_map_light_transition_start_light_matrix.t[i])
                    * g_battle_map_light_transition_frame / g_battle_map_light_transition_duration;
        }
        SetColorMatrix(&g_battle_map_light_color_matrix);
        g_battle_map_light_transition_frame++;
        if (g_battle_map_light_transition_frame > g_battle_map_light_transition_duration) {
            g_battle_map_light_transition_command = 0;
        }
        break;
    case MAP_LIGHT_MATRIX_COMMAND_START_DELTA:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_battle_map_light_color_matrix.m[i][j] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                    + g_battle_map_light_transition_target_color_matrix.m[i][j] * g_battle_map_light_transition_frame
                        / g_battle_map_light_transition_duration;
                g_battle_map_light_matrix.m[i][j] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                    + g_battle_map_light_transition_target_light_matrix.m[i][j] * g_battle_map_light_transition_frame
                        / g_battle_map_light_transition_duration;
            }
            g_battle_map_light_color_matrix.t[i] = g_battle_map_light_transition_start_color_matrix.m[i][j]
                + g_battle_map_light_transition_frame * g_battle_map_light_transition_target_color_matrix.t[i]
                    / g_battle_map_light_transition_duration;
            g_battle_map_light_matrix.t[i] = g_battle_map_light_transition_start_light_matrix.m[i][j]
                + g_battle_map_light_transition_frame * g_battle_map_light_transition_target_light_matrix.t[i]
                    / g_battle_map_light_transition_duration;
        }
        SetColorMatrix(&g_battle_map_light_color_matrix);
        g_battle_map_light_transition_frame++;
        if (g_battle_map_light_transition_frame > g_battle_map_light_transition_duration) {
            g_battle_map_light_transition_command = 0;
        }
        break;
    }
    return result;
}

/* Map color state commands, selected on command - 0x56: 0x56 applies the
 * background gradient pair, 0x5a the ambient color and 0x63 the darkness
 * color; the neighboring commands save, restore or return those records.
 *
 * Only the GET arms assign `result`; the others return it uninitialized, as
 * the target does. */
void* battle_map_light_state_command(map_light_command_e command, void* data) {
    void* result;

    switch (command) {
    case MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP]
            = ((map_background_gradient_colors_t*)data)->colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM]
            = ((map_background_gradient_colors_t*)data)->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT:
        result = &g_battle_map_background_gradient_colors;
        break;
    case MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT:
        g_battle_map_saved_background_gradient_first
            = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_battle_map_saved_background_gradient_second
            = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP]
            = g_battle_map_saved_background_gradient_first;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM]
            = g_battle_map_saved_background_gradient_second;
        break;
    case MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR:
        g_battle_map_back_color_red = ((CVECTOR*)data)->r;
        g_battle_map_back_color_green = ((CVECTOR*)data)->g;
        g_battle_map_back_color_blue = ((CVECTOR*)data)->b;
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        break;
    case MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR:
        g_battle_map_saved_back_color_red = g_battle_map_back_color_red;
        g_battle_map_saved_back_color_green = g_battle_map_back_color_green;
        g_battle_map_saved_back_color_blue = g_battle_map_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR:
        g_battle_map_back_color_red = g_battle_map_saved_back_color_red;
        g_battle_map_back_color_green = g_battle_map_saved_back_color_green;
        g_battle_map_back_color_blue = g_battle_map_saved_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR: {
        CVECTOR* color = &g_battle_map_cached_back_color;

        color->r = g_battle_map_back_color_red;
        g_battle_map_cached_back_color.g = g_battle_map_back_color_green;
        g_battle_map_cached_back_color.b = g_battle_map_back_color_blue;
        result = color;
        break;
    }
    case MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR:
        g_battle_map_current_darkness_color.r = ((CVECTOR*)data)->r;
        g_battle_map_current_darkness_color.g = ((CVECTOR*)data)->g;
        g_battle_map_current_darkness_color.b = ((CVECTOR*)data)->b;
        break;
    case MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR:
        g_battle_map_current_darkness_color.r = 0;
        g_battle_map_current_darkness_color.g = 0;
        g_battle_map_current_darkness_color.b = 1;
        break;
    case MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR:
        result = &g_battle_map_current_darkness_color;
        break;
    }
    return result;
}

/* g_battle_map_weather_splash_uv_rects: read-only table of this object (32 bytes at 0x800e6b30), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_map_weather_splash_uv_rects_rodata[8] = {
    0x00a00040,
    0x000f000f,
    0x00a00030,
    0x000f000f,
    0x00a00020,
    0x000f000f,
    0x00a00010,
    0x000f000f,
};

/* g_battle_map_weather_drop_uv_rects: read-only table of this object (24 bytes at 0x800e6b50), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_map_weather_drop_uv_rects_rodata[6] = {
    0x00a00010,
    0x000f000f,
    0x00a00020,
    0x000f000f,
    0x00a00030,
    0x000f000f,
};

/* g_battle_map_white_cvector: read-only table of this object (8 bytes at 0x800e6b68), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_map_white_cvector_rodata[2] = {
    0x00ffffff,
    0x00000000,
};

#define GT3(i)       (g_battle_gfx_current_render_buffer->gt3[i])
#define GT4(i)       (g_battle_gfx_current_render_buffer->gt4[i])
#define SAVED_GT3(i) (g_battle_target_tile_color_buffer->gt3[i])
#define SAVED_GT4(i) g_battle_target_tile_color_buffer->gt4[i]

/* Queue the map mesh parts and weather particles for one frame, then apply or
 * restore the targeting and movement tile colors. */
void battle_map_draw_mesh_and_weather(MATRIX* camera) {
    MATRIX light_rotation;
    MATRIX light_matrix;
    MATRIX effect_matrix;
    RECT splash_uv[4];
    RECT drop_uv[3];
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

    memcpy(splash_uv, g_battle_map_weather_splash_uv_rects, sizeof(splash_uv));
    memcpy(drop_uv, g_battle_map_weather_drop_uv_rects, sizeof(drop_uv));
    unused = g_battle_map_white_cvector;
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay_modes[1]);
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay);
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay_modes[0]);
    battle_map_update_texture_animations();
    battle_map_update_mesh_part_animations();
    if (g_battle_map_lightning_mode != MAP_LIGHTNING_OFF && g_battle_sound_weather_sfx_enabled != 0) {
        if (rand() % 480 == 0) {
            g_battle_map_lightning_state.timer = 60;
            if (rand() & 1) {
                g_battle_sound_secondary_weather_sfx_id
                    = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_A);
                main_sound_play_sfx_find_channel(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_A));
            } else {
                g_battle_sound_tertiary_weather_sfx_id
                    = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_B);
                main_sound_play_sfx_find_channel(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_B));
            }
        }
        if (g_battle_map_lightning_state.timer != 0) {
            u32 remaining;

            remaining = --g_battle_map_lightning_state.timer;
            g_battle_map_lightning_state.colors[0].r = g_battle_map_lightning_state.base_colors[0].r
                - g_battle_map_lightning_state.base_colors[0].r * remaining / 60 + remaining * 208 / 60;
            g_battle_map_lightning_state.colors[0].g = g_battle_map_lightning_state.base_colors[0].g
                - g_battle_map_lightning_state.base_colors[0].g * remaining / 60 + remaining * 208 / 60;
            g_battle_map_lightning_state.colors[0].b = g_battle_map_lightning_state.base_colors[0].b
                - g_battle_map_lightning_state.base_colors[0].b * remaining / 60 + remaining * 3;
            g_battle_map_lightning_state.colors[1].r = g_battle_map_lightning_state.base_colors[1].r
                - g_battle_map_lightning_state.base_colors[1].r * remaining / 60 + remaining * 112 / 60;
            g_battle_map_lightning_state.colors[1].g = g_battle_map_lightning_state.base_colors[1].g
                - g_battle_map_lightning_state.base_colors[1].g * remaining / 60 + remaining * 112 / 60;
            g_battle_map_lightning_state.colors[1].b = g_battle_map_lightning_state.base_colors[1].b
                - g_battle_map_lightning_state.base_colors[1].b * remaining / 60 + remaining * 96 / 60;
            battle_map_light_state_command(
                MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &g_battle_map_lightning_state.colors[0]);
        } else {
            battle_map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
            if (g_battle_map_lightning_mode == MAP_LIGHTNING_ONCE) {
                g_battle_map_lightning_mode = MAP_LIGHTNING_OFF;
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
    g_battle_gfx_current_render_buffer->overlay.r0
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_battle_gfx_current_render_buffer->overlay.g0
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_battle_gfx_current_render_buffer->overlay.b0
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_battle_gfx_current_render_buffer->overlay.r1
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_battle_gfx_current_render_buffer->overlay.g1
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_battle_gfx_current_render_buffer->overlay.b1
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_battle_gfx_current_render_buffer->overlay.r2
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_battle_gfx_current_render_buffer->overlay.g2
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_battle_gfx_current_render_buffer->overlay.b2
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    g_battle_gfx_current_render_buffer->overlay.r3
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_battle_gfx_current_render_buffer->overlay.g3
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_battle_gfx_current_render_buffer->overlay.b3
        = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    MulMatrix0(&g_battle_map_light_matrix, &g_battle_map_light_rotation_matrix, &g_battle_map_local_light_matrix);
    SetLightMatrix(&g_battle_map_local_light_matrix);
    if (g_battle_map_polygon_flag_clear_countdown != 0) {
        if (--g_battle_map_polygon_flag_clear_countdown == 0) {
            battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE);
        }
    }
    SetRotMatrix(camera);
    SetTransMatrix(camera);
    g_battle_map_part_textured_triangle_count = g_battle_map_mesh_parts[0].textured_triangle_count;
    g_battle_map_part_textured_triangle_positions
        = &g_battle_map_textured_triangle_positions[g_battle_map_mesh_parts[0].textured_triangle_start];
    g_battle_map_part_textured_triangle_normals
        = &g_battle_map_textured_triangle_normals[g_battle_map_mesh_parts[0].textured_triangle_start];
    map_queue_textured_triangles(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->gt3[g_battle_map_mesh_parts[0].textured_triangle_start],
        g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts[0].textured_quad_count;
    g_battle_map_part_textured_quad_positions
        = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts[0].textured_quad_start];
    g_battle_map_part_textured_quad_normals
        = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts[0].textured_quad_start];
    map_queue_textured_quads(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->gt4[g_battle_map_mesh_parts[0].textured_quad_start],
        g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts[0].untextured_triangle_count;
    g_battle_map_part_untextured_triangle_positions
        = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts[0].untextured_triangle_start];
    map_queue_untextured_triangles(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->f3[g_battle_map_mesh_parts[0].untextured_triangle_start],
        g_battle_map_camera_rotation.vy, &i);
    g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts[0].untextured_quad_count;
    g_battle_map_part_untextured_quad_positions
        = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts[0].untextured_quad_start];
    map_queue_untextured_quads(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->f4[g_battle_map_mesh_parts[0].untextured_quad_start],
        g_battle_map_camera_rotation.vy, &i);
    /* Dead store: `matrix` is reassigned for every drawn child part before
     * use. It still survives the first CSE pass (the loop uses `matrix`), and
     * the second pass reuses it for the loop-hoisted &g_battle_map_mesh_parts[0].matrix
     * constant; reload then deletes that spilled constant together with this
     * set. The transient local moves the draw calls' `&i` temporary from v0
     * to a0, as in the target. Earlier than the textured_triangle_count access, it would
     * become the base of that address instead. */
    matrix = &g_battle_map_mesh_parts[0].matrix;
    PushMatrix();
    for (part = 1; part < 9; part++) {
        if (g_battle_map_mesh_animation_instructions[part].states[0][1].next != 0) {
            u8 parent;

            rotation = &g_battle_map_mesh_parts[part].rotation;
            matrix = &g_battle_map_mesh_parts[part].matrix;
            parent = g_battle_map_mesh_parts[part].parent;
            RotMatrix(rotation, matrix);
            MulMatrix2(&g_battle_map_mesh_parts[parent].matrix, matrix);
            SetRotMatrix(&g_battle_map_mesh_parts[parent].matrix);
            SetTransMatrix(&g_battle_map_mesh_parts[parent].matrix);
            RotTrans(&g_battle_map_mesh_parts[part].translation, (VECTOR*)matrix->t, &flag);
            ScaleMatrix(matrix, &g_battle_map_mesh_parts[part].scale);
            RotMatrix(rotation, &light_rotation);
            MulMatrix0(&g_battle_map_local_light_matrix, &light_rotation, &light_matrix);
            SetLightMatrix(&light_matrix);
            SetRotMatrix(matrix);
            SetTransMatrix(matrix);
            g_battle_map_part_textured_triangle_count = g_battle_map_mesh_parts[part].textured_triangle_count;
            g_battle_map_part_textured_triangle_positions
                = &g_battle_map_textured_triangle_positions[g_battle_map_mesh_parts[part].textured_triangle_start];
            g_battle_map_part_textured_triangle_normals
                = &g_battle_map_textured_triangle_normals[g_battle_map_mesh_parts[part].textured_triangle_start];
            map_queue_textured_triangles(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->gt3[g_battle_map_mesh_parts[part].textured_triangle_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts[part].textured_quad_count;
            g_battle_map_part_textured_quad_positions
                = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts[part].textured_quad_start];
            g_battle_map_part_textured_quad_normals
                = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts[part].textured_quad_start];
            map_queue_textured_quads(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->gt4[g_battle_map_mesh_parts[part].textured_quad_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts[part].untextured_triangle_count;
            g_battle_map_part_untextured_triangle_positions
                = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts[part].untextured_triangle_start];
            map_queue_untextured_triangles(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->f3[g_battle_map_mesh_parts[part].untextured_triangle_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts[part].untextured_quad_count;
            g_battle_map_part_untextured_quad_positions
                = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts[part].untextured_quad_start];
            map_queue_untextured_quads(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->f4[g_battle_map_mesh_parts[part].untextured_quad_start],
                g_battle_map_camera_rotation.vy, &i);
            if (g_battle_map_untextured_otz_mask != 0 && g_battle_effect_map_vertices != 0
                && (g_battle_map_loaded_id == MAP_ID_ENTRANCE_TO_DEATH_CITY
                    || g_battle_map_loaded_id == MAP_ID_GRAVEYARD_OF_AIRSHIPS
                    || g_battle_map_loaded_id == MAP_ID_ZIREKILE_FALLS)) {
                RotMatrix(&g_battle_map_mesh_parts[part].rotation, &effect_matrix);
                effect_translation.vx = g_battle_map_mesh_parts[part].translation.vx;
                effect_translation.vy = g_battle_map_mesh_parts[part].translation.vy;
                effect_translation.vz = g_battle_map_mesh_parts[part].translation.vz;
                TransMatrix(&effect_matrix, &effect_translation);
                ScaleMatrix(&effect_matrix, &g_battle_map_mesh_parts[part].scale);
                SetRotMatrix(&effect_matrix);
                SetTransMatrix(&effect_matrix);
                for (j = 0; j < 4; j++) {
                    effect_base = (part - 1) * 4 + 0x61;
                    RotTrans(&g_battle_map_mesh_animation_keyframes[j + effect_base].translation,
                        g_battle_effect_map_vertices + j + (part - 1) * 4, &effect_flag);
                }
            }
        }
    }
    PopMatrix();
    if (g_battle_map_weather_effect_mode == MAP_WEATHER_EFFECT_SNOW) {
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
            tile = (g_battle_map_weather_particles[i].z / MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                * 12);
            if (g_battle_render_buffers[0].ft3[i].y0 >= 0x119) {
                if (g_battle_map_weather_scale.vy != 0) {
                    g_battle_map_weather_particles[i].y = -0x100000 / g_battle_map_weather_scale.vy;
                }
                g_battle_map_weather_particles[i].fall_speed = 0;
                sprite = rand() % 3;

                g_battle_render_buffers[0].ft3[i].u0 = drop_uv[sprite].x;
                g_battle_render_buffers[1].ft3[i].u0 = drop_uv[sprite].x;
                g_battle_render_buffers[0].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[1].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[0].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[1].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[0].ft3[i].v0 = drop_uv[sprite].y;
                g_battle_render_buffers[1].ft3[i].v0 = drop_uv[sprite].y;
                g_battle_render_buffers[0].ft3[i].v1 = drop_uv[sprite].y;
                g_battle_render_buffers[1].ft3[i].v1 = drop_uv[sprite].y;
                g_battle_render_buffers[0].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                g_battle_render_buffers[1].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
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
            g_battle_map_weather_particles[i].y
                += g_battle_map_weather_particles[i].fall_speed * g_main_system_animation_speed;
            RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_battle_gfx_current_render_buffer->ft3[i].x1 = sx + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].x2 = sx + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].y2 = sy + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].x0 = sx;
                g_battle_gfx_current_render_buffer->ft3[i].y0 = sy;
                g_battle_gfx_current_render_buffer->ft3[i].y1 = sy;
                AddPrim(g_battle_gfx_current_render_buffer->otag + 2, &g_battle_gfx_current_render_buffer->ft3[i]);
            }
        }
        PopMatrix();
    }
    if (g_battle_map_weather_effect_mode == MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
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
            tile = (g_battle_map_weather_particles[i].z / MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                * 12);
            tile = (g_battle_map_weather_particles[i + 32].z / MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i + 32].x / MAP_TILE_SIZE;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                * 12);
            if ((g_battle_map_command_0x96_frame < g_battle_map_command_0x96_duration / 8
                    && g_battle_render_buffers[0].ft3[i].y0 >= 0x119)
                || (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame
                    && g_battle_render_buffers[0].ft3[i].y0 < 0)) {
                if (g_battle_map_weather_scale.vy != 0) {
                    if (g_battle_map_command_0x96_frame < g_battle_map_command_0x96_duration / 8) {
                        g_battle_map_weather_particles[i].y = -0x100000 / g_battle_map_weather_scale.vy;
                    }
                    if (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame) {
                        g_battle_map_weather_particles[i].y = 0x30000 / g_battle_map_weather_scale.vy;
                    }
                }
                sprite = rand() % 3;
                g_battle_map_weather_particles[i].fall_speed = g_battle_map_weather_fall_speed + 3 - sprite;

                g_battle_render_buffers[0].ft3[i].u0 = drop_uv[sprite].x;
                g_battle_render_buffers[1].ft3[i].u0 = drop_uv[sprite].x;
                g_battle_render_buffers[0].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[1].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[0].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[1].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_battle_render_buffers[0].ft3[i].v0 = drop_uv[sprite].y;
                g_battle_render_buffers[1].ft3[i].v0 = drop_uv[sprite].y;
                g_battle_render_buffers[0].ft3[i].v1 = drop_uv[sprite].y;
                g_battle_render_buffers[1].ft3[i].v1 = drop_uv[sprite].y;
                g_battle_render_buffers[0].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                g_battle_render_buffers[1].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
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
                g_battle_map_weather_particles[i].y += g_battle_map_weather_particles[i].fall_speed
                    * g_main_system_animation_speed
                    * rsin((g_battle_map_command_0x96_frame << 13) / g_battle_map_command_0x96_duration + 0x400) / 2048;
            }
            if (g_battle_map_command_0x96_duration / 8 < g_battle_map_command_0x96_frame) {
                g_battle_map_weather_particles[i].y
                    -= g_battle_map_weather_particles[i].fall_speed * g_main_system_animation_speed;
            }
            RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_battle_gfx_current_render_buffer->ft3[i].x1 = sx + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].x2 = sx + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].y2 = sy + half_size;
                g_battle_gfx_current_render_buffer->ft3[i].x0 = sx;
                g_battle_gfx_current_render_buffer->ft3[i].y0 = sy;
                g_battle_gfx_current_render_buffer->ft3[i].y1 = sy;
                AddPrim(g_battle_gfx_current_render_buffer->otag + 2, &g_battle_gfx_current_render_buffer->ft3[i]);
            }
        }
        if (g_battle_map_command_0x96_frame == g_battle_map_command_0x96_duration) {
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
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
    if (g_battle_map_weather_effect_mode == MAP_WEATHER_EFFECT_RAIN) {
        half_size = g_battle_map_camera_zoom.vx * 22;
        half_size /= 0x2000;
        for (i = 0; i < 32; i++) {
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (g_battle_map_weather_particles[i].z / MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_battle_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -(
                (g_battle_map_tile_data_ptr[tile].height + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                * 12);
            if (ground < g_battle_map_weather_particles[i + 32].y) {
                g_battle_map_weather_particles[i].fall_speed = 0;
                g_battle_map_weather_particles[i].x = rand() % (g_battle_map_tile_width * MAP_TILE_SIZE);
                g_battle_map_weather_particles[i].z = rand() % (g_battle_map_tile_depth * MAP_TILE_SIZE);
                spawn_y = -0x180;
                g_battle_map_weather_particles[i].y = spawn_y;
                g_battle_map_weather_particles[i + 32].x = g_battle_map_weather_particles[i].x;
                g_battle_map_weather_particles[i + 32].z = g_battle_map_weather_particles[i].z;
                g_battle_map_weather_particles[i + 32].y
                    = spawn_y - g_battle_map_weather_fall_speed - rand() % (g_battle_map_weather_primary_speed * 12);
            }
            g_battle_map_weather_particles[i].y += g_battle_map_weather_fall_speed * g_main_system_animation_speed;
            g_battle_map_weather_particles[i + 32].y += g_battle_map_weather_fall_speed * g_main_system_animation_speed;
            if (i < 16) {
                g_battle_map_weather_particles[i].y
                    += g_battle_map_weather_primary_speed * g_main_system_animation_speed;
                g_battle_map_weather_particles[i + 32].y
                    += g_battle_map_weather_primary_speed * g_main_system_animation_speed;
            } else {
                g_battle_map_weather_particles[i].y
                    += g_battle_map_rain_secondary_fall_speed * g_main_system_animation_speed;
                g_battle_map_weather_particles[i + 32].y
                    += g_battle_map_rain_secondary_fall_speed * g_main_system_animation_speed;
            }
            if (g_battle_map_weather_particles[i].y >= -0x17F) {
                if (ground < g_battle_map_weather_particles[i].y) {
                    g_battle_map_weather_particles[i].y = g_battle_map_lowest_surface_y;
                } else if (g_battle_map_weather_particles[i + 32].y < ground) {
                    RotTrans((SVECTOR*)&g_battle_map_weather_particles[i], &screen, &flag);
                    sx = screen.vx;
                    sy = screen.vy;
                    g_battle_gfx_current_render_buffer->ft3[i].x2 = sx + 1;
                    g_battle_gfx_current_render_buffer->ft3[i].x0 = sx;
                    g_battle_gfx_current_render_buffer->ft3[i].y0 = sy;
                    g_battle_gfx_current_render_buffer->ft3[i].y2 = sy - 1;
                    RotTrans((SVECTOR*)&g_battle_map_weather_particles[i + 32], &screen, &flag);
                    depth = screen.vz / 4;
                    if (depth > 0 && depth < 0x180) {
                        sx = screen.vx;
                        sy = screen.vy;
                        g_battle_gfx_current_render_buffer->ft3[i].x1 = sx;
                        g_battle_gfx_current_render_buffer->ft3[i].y1 = sy;
                        AddPrim(g_battle_gfx_current_render_buffer->otag + depth,
                            &g_battle_gfx_current_render_buffer->ft3[i]);
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
                if (!(g_battle_map_tile_data_ptr[tile].flags.untargetable)) {
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
                    g_battle_gfx_current_render_buffer->ft3[i + 32].x0 = sx - half_size;
                    g_battle_gfx_current_render_buffer->ft3[i + 32].y0 = splash_y - half_size;
                    g_battle_gfx_current_render_buffer->ft3[i + 32].x1 = sx + half_size;
                    g_battle_gfx_current_render_buffer->ft3[i + 32].y1 = splash_y + half_size;
                    g_battle_gfx_current_render_buffer->ft3[i + 32].x2 = sx - half_size;
                    g_battle_gfx_current_render_buffer->ft3[i + 32].y2 = splash_y + half_size;
                    if (g_battle_map_weather_particles[i].fall_speed == 1) {
                        AddPrim(g_battle_gfx_current_render_buffer->otag + depth,
                            &g_battle_gfx_current_render_buffer->ft3[i + 32]);
                    }
                }
                g_battle_map_rain_splash_particles[i].fall_speed--;
                frame = g_battle_map_rain_splash_particles[i].fall_speed;
                if ((frame & 3) == 0) {
                    g_battle_render_buffers[0].ft3[i + 32].u0 = splash_uv[(frame >> 2) & 3].x;
                    g_battle_render_buffers[1].ft3[i + 32].u0 = splash_uv[(frame >> 2) & 3].x;
                    g_battle_render_buffers[0].ft3[i + 32].u1
                        = splash_uv[(frame >> 2) & 3].x + splash_uv[(frame >> 2) & 3].w;
                    g_battle_render_buffers[1].ft3[i + 32].u1
                        = splash_uv[(frame >> 2) & 3].x + splash_uv[(frame >> 2) & 3].w;
                    g_battle_render_buffers[0].ft3[i + 32].u2 = splash_uv[(frame >> 2) & 3].x;
                    g_battle_render_buffers[1].ft3[i + 32].u2 = splash_uv[(frame >> 2) & 3].x;
                    g_battle_render_buffers[0].ft3[i + 32].v0 = splash_uv[(frame >> 2) & 3].y;
                    g_battle_render_buffers[1].ft3[i + 32].v0 = splash_uv[(frame >> 2) & 3].y;
                    g_battle_render_buffers[0].ft3[i + 32].v1
                        = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_battle_render_buffers[1].ft3[i + 32].v1
                        = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_battle_render_buffers[0].ft3[i + 32].v2
                        = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_battle_render_buffers[1].ft3[i + 32].v2
                        = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                }
                if (g_battle_map_rain_splash_particles[i].fall_speed == 0) {
                    g_battle_map_weather_particles[i].fall_speed = 0;
                }
            }
        }
    }
    /* The ambient color restores use pointer arithmetic on the decayed arrays
     * rather than GT3(i)/GT4(i) indexing: that keeps the target's
     * `addu offset, base` operand order (as in target_set_tile_background_color). */
    if (g_battle_map_ambient_restored_buffer != 0) {
        if (g_battle_map_ambient_restored_buffer != g_battle_gfx_current_render_buffer) {
            for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
                if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                }
            }
            for (i = 0; i < g_battle_map_textured_quad_count; i++) {
                if (!(g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                }
            }
        }
        g_battle_map_ambient_restored_buffer = 0;
    }
    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    if (g_battle_target_tile_mark_modes[0] == BATTLE_TILE_MARK_SHOW_MOVE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        g_battle_target_tile_mark_modes[0] = BATTLE_TILE_MARK_MOVE_SHOWN;
    }
    if (g_battle_target_tile_mark_modes[1] == BATTLE_TILE_MARK_SHOW_RANGE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        g_battle_target_tile_mark_modes[1] = BATTLE_TILE_MARK_RANGE_SHOWN;
    }
    if (g_battle_target_tile_mark_modes[2] == BATTLE_TILE_MARK_SHOW_TARGETS) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
        g_battle_target_tile_mark_modes[2] = BATTLE_TILE_MARK_TARGETS_SHOWN;
    }
    if (g_battle_target_tile_mark_modes[0] == BATTLE_TILE_MARK_HIDE_MOVE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
            if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                GT3(i).r0 = SAVED_GT3(i).r0;
                GT3(i).r1 = SAVED_GT3(i).r1;
                GT3(i).r2 = SAVED_GT3(i).r2;
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
            if (!(g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                GT4(i).r0 = SAVED_GT4(i).r0;
                GT4(i).r1 = SAVED_GT4(i).r1;
                GT4(i).r2 = SAVED_GT4(i).r2;
                GT4(i).r3 = SAVED_GT4(i).r3;
            }
        }
        g_battle_target_tile_mark_modes[0] = BATTLE_TILE_MARK_NONE;
        if ((g_battle_target_tile_mark_modes[1] | g_battle_target_tile_mark_modes[2]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_battle_target_tile_mark_modes[1] == BATTLE_TILE_MARK_HIDE_RANGE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
        }
        g_battle_target_tile_mark_modes[1] = BATTLE_TILE_MARK_NONE;
        if ((g_battle_target_tile_mark_modes[0] | g_battle_target_tile_mark_modes[2]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_battle_target_tile_mark_modes[2] == BATTLE_TILE_MARK_HIDE_TARGETS) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
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
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
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
        g_battle_target_tile_mark_modes[2] = BATTLE_TILE_MARK_NONE;
        if ((g_battle_target_tile_mark_modes[0] | g_battle_target_tile_mark_modes[1]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
}

void battle_target_set_tile_background_color(s32 mode, s32 unused) {
    s32 i;
    s16 other_state_a;
    s16 other_state_b;
    s16 tile;
    map_tile_marks_t marks;

    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    switch (mode) {
    case BATTLE_TILE_MARK_NONE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            g_battle_gfx_current_render_buffer->gt3[i].clut
                = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            g_battle_gfx_current_render_buffer->gt4[i].clut
                = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }
        g_battle_target_tile_mark_modes[0] = BATTLE_TILE_MARK_HIDE_MOVE;
        g_battle_target_tile_mark_modes[1] = BATTLE_TILE_MARK_HIDE_RANGE;
        g_battle_target_tile_mark_modes[2] = BATTLE_TILE_MARK_HIDE_TARGETS;
        battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT);
        g_battle_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_MOVE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x28;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x60;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x28;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x60;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_battle_target_tile_mark_modes[0] = BATTLE_TILE_MARK_SHOW_MOVE;
        other_state_a = g_battle_target_tile_mark_modes[1];
        other_state_b = g_battle_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_RANGE && other_state_b == BATTLE_TILE_MARK_HIDE_TARGETS)) {
            battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_MOVE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }
        g_battle_target_tile_mark_modes[0] = BATTLE_TILE_MARK_HIDE_MOVE;
        g_battle_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_RANGE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x18;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x20;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x18;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x20;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x20;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_battle_target_tile_mark_modes[1] = BATTLE_TILE_MARK_SHOW_RANGE;
        other_state_a = g_battle_target_tile_mark_modes[0];
        other_state_b = g_battle_target_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_MOVE && other_state_b == BATTLE_TILE_MARK_HIDE_TARGETS)) {
            battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_RANGE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                        (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }
        g_battle_target_tile_mark_modes[1] = BATTLE_TILE_MARK_HIDE_RANGE;
        g_battle_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    case BATTLE_TILE_MARK_SHOW_TARGETS:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_battle_gfx_current_render_buffer->gt3[i].clut
                        = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x50;
                    g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x10;
                    g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x10;
                    g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x10;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_battle_gfx_current_render_buffer->gt4[i].clut
                        = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                        | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x50;
                    g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x60;
                    g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x10;
                    g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x10;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }
        g_battle_target_tile_mark_modes[2] = BATTLE_TILE_MARK_SHOW_TARGETS;
        other_state_a = g_battle_target_tile_mark_modes[0];
        other_state_b = g_battle_target_tile_mark_modes[1];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_TILE_MARK_HIDE_MOVE && other_state_b == BATTLE_TILE_MARK_HIDE_RANGE)) {
            battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_TILE_MARK_HIDE_TARGETS:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_battle_gfx_current_render_buffer->gt3[i].clut
                            = (g_battle_gfx_current_render_buffer->gt3[i].clut & 0x803f)
                            | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_battle_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_battle_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        }
                    } else {
                        g_battle_gfx_current_render_buffer->gt3[i].r0 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].r1 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].r2 = 0x60;
                        g_battle_gfx_current_render_buffer->gt3[i].g0 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].g1 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].g2 = 0x18;
                        g_battle_gfx_current_render_buffer->gt3[i].b0 = 0x20;
                        g_battle_gfx_current_render_buffer->gt3[i].b1 = 0x20;
                        g_battle_gfx_current_render_buffer->gt3[i].b2 = 0x20;
                    }
                }
            }
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_battle_target_color_tile_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_target_color_tile_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_target_color_tile_y * g_battle_map_tile_width + g_battle_target_color_tile_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_battle_gfx_current_render_buffer->gt4[i].clut
                            = (g_battle_gfx_current_render_buffer->gt4[i].clut & 0x803f)
                            | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_battle_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_battle_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                            (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                        }
                    } else {
                        g_battle_gfx_current_render_buffer->gt4[i].r0 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r1 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r2 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].r3 = 0x60;
                        g_battle_gfx_current_render_buffer->gt4[i].g0 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g1 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g2 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].g3 = 0x18;
                        g_battle_gfx_current_render_buffer->gt4[i].b0 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b1 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b2 = 0x20;
                        g_battle_gfx_current_render_buffer->gt4[i].b3 = 0x20;
                    }
                }
            }
        }
        g_battle_target_tile_mark_modes[2] = BATTLE_TILE_MARK_HIDE_TARGETS;
        g_battle_target_tile_color_buffer = g_battle_gfx_current_render_buffer;
        break;
    }
}

/* g_battle_map_zero_svector: read-only table of this object (8 bytes at 0x800e6b94), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_map_zero_svector_rodata[2] = {
    0x00000000,
    0x00000000,
};

/* g_battle_map_unit_scale_vector: read-only table of this object (20 bytes at 0x800e6b9c), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_map_unit_scale_vector_rodata[5] = {
    0x00001000,
    0x00001000,
    0x00001000,
    0x00000000,
    0x00000000,
};

/* Initializes both battle render buffers: every primitive in each pool gets its
 * GPU command code, the full-screen Gouraud overlay gets its corners and two
 * semi-transparent draw modes, and the ordering table pointers are bound.
 *
 * The function-scope counters are intentionally reused by each pool loop:
 * their lifetime reproduces the target register allocation. */
void battle_gfx_init_render_buffers(void) {
    POLY_F3 prototype;
    s32 primitive_index;
    s32 buffer_index;

    g_battle_gfx_current_render_buffer = &g_battle_render_buffers[0];
    g_battle_render_buffers[0].otag = g_main_gfx_battle_otags[0];
    g_battle_render_buffers[1].otag = g_main_gfx_battle_otags[1];
    {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            for (primitive_index = 0; primitive_index < 64; primitive_index++) {
                SetPolyFT3(&g_battle_render_buffers[buffer_index].ft3[primitive_index]);
            }
        }
    }
    SetPolyF3(&prototype);
    g_battle_map_current_darkness_color.r = 0;
    g_battle_map_current_darkness_color.g = 0;
    g_battle_map_current_darkness_color.b = 1;
    g_battle_map_current_darkness_color.cd = prototype.code;
    SetPolyG4(&g_battle_render_buffers[0].overlay);
    SetPolyG4(&g_battle_render_buffers[1].overlay);
    setXY4(&g_battle_render_buffers[0].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    setXY4(&g_battle_render_buffers[1].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    {
        for (primitive_index = 0; primitive_index < 360; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT3(&g_battle_render_buffers[buffer_index].gt3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 710; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT4(&g_battle_render_buffers[buffer_index].gt4[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 64; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF3(&g_battle_render_buffers[buffer_index].f3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < 256; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF4(&g_battle_render_buffers[buffer_index].f4[primitive_index]);
            }
        }
    }
    g_battle_sound_primary_weather_sfx_id = 0;
    g_battle_sound_weather_sfx_enabled = 0;
    g_battle_effect_map_vertices = 0;
}

void battle_map_reset_mesh_render_state(void) {
    SVECTOR zero_vector;
    VECTOR unit_scale;
    s32 i;
    s16 no_tile;
    s32 j;
    s32* weather_flags;

    zero_vector = g_battle_map_zero_svector;
    unit_scale = g_battle_map_unit_scale_vector;
    g_battle_map_tile_width = 1;
    g_battle_map_tile_depth = 1;
    g_battle_target_color_tile_x = 1;
    g_battle_target_color_tile_y = 1;
    g_battle_map_untextured_otz_mask = 0;
    g_battle_map_untextured_fixed_ot_index = 4;
    g_battle_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            g_battle_unknown_800f70b4[i][j] = 0;
        }
        g_battle_unknown_800f78e8[i] = 0;
    }
    for (i = 0; i < GNS_RESOURCE_TABLE_COUNT; i++) {
        bzero(g_battle_map_loaded_gns_records[i].load_info.bytes, sizeof(gns_load_info_t));
        bzero(&g_battle_map_pending_gns_records[i].resource_type, 14);
    }
    battle_map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
    g_battle_map_lightning_mode = MAP_LIGHTNING_OFF;
    weather_flags = &g_battle_map_weather_flags;
    *weather_flags &= 0x7d;
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    g_battle_map_ambient_polygon_color.r = 0x80;
    g_battle_map_ambient_polygon_color.g = 0x80;
    g_battle_map_ambient_polygon_color.b = 0x80;
    for (i = 0; i < 360; i++) {
        g_battle_map_textured_triangle_positions[i].polygon_flags
            = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 710; i++) {
        g_battle_map_textured_quad_positions[i].polygon_flags
            = (s16)(MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE);
    }
    for (i = 0; i < 64; i++) {
        g_battle_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < 256; i++) {
        g_battle_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        g_battle_map_texture_animations[i].active = 0;
    }
    /* A signed sentinel local retains each array's original -2 constant load. */
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 360; i++) {
        g_battle_map_textured_triangle_positions[i].terrain_tile = no_tile;
    }
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < 710; i++) {
        g_battle_map_textured_quad_positions[i].terrain_tile = no_tile;
    }
    for (i = 1; i < 9; i++) {
        g_battle_map_mesh_animation_instructions[i].states[0][1].next = 0;
        g_battle_map_mesh_parts[i].speed = 0x10;
        g_battle_map_mesh_parts[i].keyframe = 0;
        g_battle_map_mesh_part_animation_states[i] = 0;
    }
    g_battle_map_textured_triangle_count = 0;
    g_battle_map_textured_quad_count = 0;
    g_battle_map_untextured_triangle_count = 0;
    g_battle_map_untextured_quad_count = 0;
    for (i = 0; i < 9; i++) {
        g_battle_map_mesh_parts[i].textured_triangle_count = 0;
        g_battle_map_mesh_parts[i].textured_quad_count = 0;
        g_battle_map_mesh_parts[i].untextured_triangle_count = 0;
        g_battle_map_mesh_parts[i].untextured_quad_count = 0;
        g_battle_map_mesh_parts[i].rotation = zero_vector;
        g_battle_map_mesh_parts[i].rotation_start = zero_vector;
        g_battle_map_mesh_parts[i].rotation_step = zero_vector;
        g_battle_map_mesh_parts[i].translation = zero_vector;
        g_battle_map_mesh_parts[i].translation_start = zero_vector;
        g_battle_map_mesh_parts[i].translation_step = zero_vector;
        g_battle_map_mesh_parts[i].scale = unit_scale;
        g_battle_map_mesh_parts[i].scale_start = unit_scale;
        g_battle_map_mesh_parts[i].scale_step = unit_scale;
        g_battle_map_mesh_parts[i].next_instruction = 1;
        g_battle_map_mesh_part_animation_states[i] = 0;
    }
}

/* Rebuilds the camera matrix from the angles (`rotation`, an SVECTOR), the
 * screen offset and the zoom.
 *
 * The matrix is the rotation scaled by the zoom, with a translation that puts
 * g_battle_camera_integer_coords (the negated camera position) at the screen
 * offset. It is stored in *camera_matrix, g_battle_map_camera_matrix and map mesh
 * part 0; the angles and zoom are kept in g_battle_map_camera_rotation and
 * g_battle_map_camera_zoom, and the light rotation matrix is rebuilt. The GTE is
 * left with the scaled rotation and the bare screen offset as its translation. */
void battle_camera_update_matrices(
    MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom) {
    long result;

    g_battle_map_camera_zoom = *camera_zoom;
    memcpy(&g_battle_map_camera_rotation, rotation, sizeof(g_battle_map_camera_rotation));

    RotMatrix(rotation, camera_matrix);
    TransMatrix(camera_matrix, screen_offset);
    ScaleMatrix(camera_matrix, &g_battle_map_camera_zoom);
    RotMatrix(rotation, &g_battle_map_camera_matrix);
    SetRotMatrix(camera_matrix);
    SetTransMatrix(camera_matrix);
    RotTrans(&g_battle_camera_integer_coords, (VECTOR*)g_battle_map_camera_matrix.t, &result);
    ScaleMatrix(&g_battle_map_camera_matrix, &g_battle_map_camera_zoom);

    *camera_matrix = g_battle_map_camera_matrix;
    g_battle_map_mesh_parts[0].matrix = g_battle_map_camera_matrix;
    RotMatrix(&g_battle_map_light_rotation, &g_battle_map_light_rotation_matrix);
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
        SetColorMatrix(g_battle_map_lightning_state.color_matrices);
        SetBackColor(g_battle_map_back_color_red, g_battle_map_back_color_green, g_battle_map_back_color_blue);
        break;
    case MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER:
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        g_battle_map_lightning_mode = MAP_LIGHTNING_OFF;
        weather &= 7;
        if (weather < BATTLE_WEATHER_RAIN) {
            battle_sound_stop_weather_sfx();
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_CLEAR);
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_RAIN) {
            if (g_battle_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_SNOW);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHT_RAIN);
            }
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STORM) {
            if (g_battle_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_SNOW);
            } else {
                g_battle_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_battle_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_RAIN);
            }
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == BATTLE_WEATHER_STRONG_STORM) {
            if (g_battle_map_weather_flags & MAP_WEATHER_FLAG_SNOW) {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_SNOW);
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
            } else {
                battle_sound_stop_weather_sfx();
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_HEAVY_RAIN);
                battle_map_run_weather_command(MAP_WEATHER_COMMAND_LIGHTNING);
                g_battle_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
                if (g_battle_sound_weather_sfx_enabled != 0) {
                    main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
                }
            }
        }
        break;
    case MAP_WEATHER_COMMAND_THUNDER:
        g_battle_map_lightning_mode = MAP_LIGHTNING_ONCE;
        g_battle_map_lightning_state.timer = 0x3c;
        g_battle_sound_primary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN);
        if (g_battle_sound_weather_sfx_enabled != 0) {
            main_sound_play_bg_sfx(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_RAIN));
        }
        g_battle_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_LIGHTNING:
        g_battle_map_lightning_mode = MAP_LIGHTNING_RANDOM;
        g_battle_map_lightning_state.timer = 0x3c;
        g_battle_map_lightning_state.color_matrices[1].m[0][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[0][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[1][2] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][0] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][1] = 0;
        g_battle_map_lightning_state.color_matrices[1].m[2][2] = 0;
        battle_map_run_weather_command(MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case MAP_WEATHER_COMMAND_SNOW:
        g_battle_map_weather_fall_speed = 1;
        g_battle_map_weather_primary_speed = 4;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_SNOW:
        g_battle_map_weather_fall_speed = 1;
        g_battle_map_weather_primary_speed = 1;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_SNOW:
        g_battle_map_weather_fall_speed = 3;
        g_battle_map_weather_primary_speed = 0x10;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_RAIN:
        g_battle_map_weather_fall_speed = 5;
        g_battle_map_weather_primary_speed = 6;
        g_battle_map_rain_secondary_fall_speed = 9;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_LIGHT_RAIN:
        g_battle_map_weather_fall_speed = 3;
        g_battle_map_weather_primary_speed = 5;
        g_battle_map_rain_secondary_fall_speed = 8;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_HEAVY_RAIN:
        g_battle_map_weather_fall_speed = 5;
        g_battle_map_weather_primary_speed = 9;
        g_battle_map_rain_secondary_fall_speed = 0x12;
        battle_map_init_weather_particles(MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case MAP_WEATHER_COMMAND_CLEAR:
        g_battle_map_weather_effect_mode = MAP_WEATHER_EFFECT_CLEAR;
        g_battle_map_lightning_mode = MAP_LIGHTNING_OFF;
        break;
    }
}

/* Flat-textured triangle; the psx/libgpu.h clean-room set does not declare it. */

/* Set up the weather particles and their flat-textured sprites.
 *
 * 0x8f (snow, from battle_map_run_weather_command 0x53/0x7e/0x7f) scatters 64 flakes;
 * 0x90 (rain, 0x55/0x7c/0x7d) scatters 32 drop pairs over the map extent and
 * records the lowest tile surface (largest y) in g_battle_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
static void battle_map_init_weather_particles(map_weather_command_e command) {
    RECT rects[3];
    s16 sprite;
    s16 tile;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    memcpy(rects, g_battle_map_weather_drop_uv_rects, sizeof(rects));
    switch (command & 0xff) {
    case MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES:
        g_battle_map_weather_effect_mode = MAP_WEATHER_EFFECT_SNOW;
        for (i = 0; i < 64; i++) {
            g_battle_map_weather_particles[i].y = -rand() % 240 - 16;
            g_battle_map_weather_particles[i].fall_speed = g_battle_map_weather_fall_speed;
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
            g_battle_map_weather_particles[i].fall_speed = sprite + g_battle_map_weather_fall_speed;
            if (rand() & 1) {
                g_battle_map_weather_particles[i].x = rand() % 140;
            } else {
                g_battle_map_weather_particles[i].x = -(rand() % 140);
            }
            if (rand() & 1) {
                g_battle_map_weather_particles[i].z = rand() % 140;
            } else {
                g_battle_map_weather_particles[i].z = -(rand() % 140);
            }
        }
        break;
    case MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES:
        g_battle_map_weather_effect_mode = MAP_WEATHER_EFFECT_RAIN;
        for (i = 0; i < 32; i++) {
            g_battle_map_weather_particles[i].x = rand() % (g_battle_map_tile_width * MAP_TILE_SIZE);
            g_battle_map_weather_particles[i].z = rand() % (g_battle_map_tile_depth * MAP_TILE_SIZE);
            g_battle_map_weather_particles[i].y = -(rand() % 384) - 384;
            g_battle_map_weather_particles[i + 32].x = g_battle_map_weather_particles[i].x;
            g_battle_map_weather_particles[i + 32].z = g_battle_map_weather_particles[i].z;
            g_battle_map_weather_particles[i + 32].y = g_battle_map_weather_particles[i].y - rand() % 48;
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
        g_battle_map_lowest_surface_y = 0x80000000;
        g_battle_map_tile_data_ptr = g_battle_map_tile_data;
        for (i = 0; i < g_battle_map_tile_width; i++) {
            for (j = 0; j < g_battle_map_tile_depth; j++) {
                tile = i + j * g_battle_map_tile_width;
                surface_y = -((g_battle_map_tile_data_ptr[tile].height
                                  + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                    * 12);
                if (g_battle_map_lowest_surface_y < surface_y) {
                    g_battle_map_lowest_surface_y = surface_y;
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
        g_battle_map_untextured_otz_mask = 0;
        g_battle_map_untextured_fixed_ot_index = result;
        break;
    case MAP_EFFECT_DATA_BEGIN:
        result = -1;
        g_battle_map_untextured_otz_mask = result;
        g_battle_map_untextured_fixed_ot_index = 0;
        break;
    default:
        result = -1;
        break;
    }
    return result;
}

/* Advance the keyframe animations of map mesh parts 1-8 by one frame, from
 * battle_map_draw_mesh_and_weather.
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
static void battle_map_update_mesh_part_animations(void) {
    s32 i;
    u8 next;
    u8 anim;
    s16 duration;
    s16 time;
    SVECTOR* current_b;
    SVECTOR* start_b;

    /* Copying translation through this invariant pointer to part 0, with the
     * destination taken first, matches the target: it hoists
     * &g_battle_map_mesh_parts[0].translation and bases the strength-reduced pointers on
     * it. */
    current_b = &g_battle_map_mesh_parts[0].translation;

    for (i = 1; i < 9; i++) {
        /* Advance the timer. A keyframe that has run its length asks for the
         * next instruction; one of duration 0 ends the animation. */
        if (g_battle_map_mesh_animation_instructions[i].states[0][1].next != 0) {
            g_battle_map_mesh_parts[i].elapsed
                += (g_battle_map_mesh_parts[i].speed >> 4) * g_main_system_animation_speed;
            if (g_battle_map_mesh_parts[i].duration < g_battle_map_mesh_parts[i].elapsed) {
                next = g_battle_map_mesh_parts[i].next_instruction;
                g_battle_map_mesh_parts[i].keyframe = 0;
                if (g_battle_map_mesh_animation_instructions[i].states[0][next].duration == 0) {
                    g_battle_map_mesh_part_animation_states[i] = 0;
                }
            }
        }
        if (g_battle_map_mesh_part_animation_states[i] != 0) {
            /* Start the next keyframe: the current values become the start
             * values and each channel takes its delta. */
            if (g_battle_map_mesh_parts[i].keyframe == 0) {
                g_battle_map_mesh_parts[i].keyframe
                    = g_battle_map_mesh_animation_instructions[i].states[0][next].keyframe;
                anim = g_battle_map_mesh_parts[i].keyframe;
                g_battle_map_mesh_parts[i].next_instruction
                    = g_battle_map_mesh_animation_instructions[i].states[0][next].next;
                g_battle_map_mesh_parts[i].duration
                    = g_battle_map_mesh_animation_instructions[i].states[0][next].duration;
                g_battle_map_mesh_parts[i].rotation_start = g_battle_map_mesh_parts[i].rotation;
                g_battle_map_mesh_parts[i].scale_start = g_battle_map_mesh_parts[i].scale;
                start_b = &g_battle_map_mesh_parts[i].translation_start;
                *start_b = current_b[i * (sizeof(map_mesh_part_t) / sizeof(SVECTOR))];
                g_battle_map_mesh_parts[i].elapsed = 1;
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vx
                        - g_battle_map_mesh_parts[i].rotation_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vy
                        - g_battle_map_mesh_parts[i].rotation_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vz
                        - g_battle_map_mesh_parts[i].rotation_start.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vx
                        - g_battle_map_mesh_parts[i].translation_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vy
                        - g_battle_map_mesh_parts[i].translation_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vz
                        - g_battle_map_mesh_parts[i].translation_start.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vx = g_battle_map_mesh_animation_keyframes[anim].scale_x
                        - g_battle_map_mesh_parts[i].scale_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vy = g_battle_map_mesh_animation_keyframes[anim].scale_y
                        - g_battle_map_mesh_parts[i].scale_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vz = g_battle_map_mesh_animation_keyframes[anim].scale_z
                        - g_battle_map_mesh_parts[i].scale_start.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vx = g_battle_map_mesh_animation_keyframes[anim].scale_x;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vy = g_battle_map_mesh_animation_keyframes[anim].scale_y;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vz = g_battle_map_mesh_animation_keyframes[anim].scale_z;
                }
            }
            /* Evaluate each channel's delta at elapsed / duration on its
             * curve: linear, sine or raised sine. */
            anim = g_battle_map_mesh_parts[i].keyframe;
            duration = g_battle_map_mesh_parts[i].duration;
            time = g_battle_map_mesh_parts[i].elapsed;
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vx
                    = g_battle_map_mesh_parts[i].translation_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vy
                    = g_battle_map_mesh_parts[i].translation_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vz
                    = g_battle_map_mesh_parts[i].translation_step.vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[0]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[1]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[2]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vx = g_battle_map_mesh_parts[i].translation_step.vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[3]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vy = g_battle_map_mesh_parts[i].translation_step.vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[4]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vz = g_battle_map_mesh_parts[i].translation_step.vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[5]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[6]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[7]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[8]
                                   - g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[0] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[0]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[0])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[1] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[1]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[1])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[2] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[2]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[2])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[3] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vx = g_battle_map_mesh_parts[i].translation_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[3]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[3])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[4] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vy = g_battle_map_mesh_parts[i].translation_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[4]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[4])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[5] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vz = g_battle_map_mesh_parts[i].translation_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[5]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[5])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[6] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[6]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[6])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[7] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[7]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[7])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[8] & MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[8]
                                    - g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim].angle_start[8])
                            + ONE)
                    >> 12;
            }
            /* Offset every channel by its start value. */
            g_battle_map_mesh_parts[i].rotation.vx += g_battle_map_mesh_parts[i].rotation_start.vx;
            g_battle_map_mesh_parts[i].rotation.vy += g_battle_map_mesh_parts[i].rotation_start.vy;
            g_battle_map_mesh_parts[i].rotation.vz += g_battle_map_mesh_parts[i].rotation_start.vz;
            g_battle_map_mesh_parts[i].translation.vx += g_battle_map_mesh_parts[i].translation_start.vx;
            g_battle_map_mesh_parts[i].translation.vy += g_battle_map_mesh_parts[i].translation_start.vy;
            g_battle_map_mesh_parts[i].translation.vz += g_battle_map_mesh_parts[i].translation_start.vz;
            g_battle_map_mesh_parts[i].scale.vx += g_battle_map_mesh_parts[i].scale_start.vx;
            g_battle_map_mesh_parts[i].scale.vy += g_battle_map_mesh_parts[i].scale_start.vy;
            g_battle_map_mesh_parts[i].scale.vz += g_battle_map_mesh_parts[i].scale_start.vz;
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
        result = g_battle_map_texture_animations[index].active;
        break;
    case MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_battle_map_texture_animations[index].mode == MAP_TEXTURE_ANIMATION_MODE_NONE) {
            if (g_battle_map_saved_texture_animation_modes[index] != 0) {
                g_battle_map_texture_animations[index].mode = g_battle_map_saved_texture_animation_modes[index];
            }
        }
        switch (g_battle_map_texture_animations[index].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP ... MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK ... MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_battle_map_texture_animation_moves[index].sx = g_battle_map_texture_animations[index].frame_x;
            g_battle_map_texture_animation_moves[index].sy = g_battle_map_texture_animations[index].frame_y;
            g_battle_map_texture_animation_moves[index].w = g_battle_map_texture_animations[index].width;
            g_battle_map_texture_animation_moves[index].h = g_battle_map_texture_animations[index].height;
            g_battle_map_texture_animation_moves[index].x0 = g_battle_map_texture_animations[index].canvas_x;
            g_battle_map_texture_animation_moves[index].y0 = g_battle_map_texture_animations[index].canvas_y;
            g_battle_map_texture_animations[index].active = 1;
            g_battle_map_texture_animations[index].timer = 0x7e;
            g_battle_map_texture_animations[index].frame = 0;
            break;
        default:
            main_system_handle_malloc_exception(2, 0x3cd);
            break;
        }
        break;
    case MAP_DATA_COMMAND_GET_3D_OBJECT_STATE:
        result = g_battle_map_mesh_part_animation_states[index];
        break;
    case MAP_DATA_COMMAND_SET_3D_OBJECT_STATE:
        entry = &g_battle_map_mesh_animation_instructions[index];
        for (i = 0; i < 17; i++) {
            entry->states[0][i] = entry->states[object_state][i];
        }
        g_battle_map_mesh_parts[index].elapsed = 2;
        g_battle_map_mesh_parts[index].duration = 0;
        g_battle_map_mesh_parts[index].next_instruction = 1;
        g_battle_map_mesh_part_animation_states[index] = (object_state << 8) | flag;
        break;
    case MAP_DATA_COMMAND_SPLIT_MESH_PARTS:
        for (i = 1; i < 9; i++) {
            g_battle_map_mesh_parts[i].textured_triangle_count
                = (g_battle_map_primary_textured_triangle_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].textured_quad_count = (g_battle_map_primary_textured_quad_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].untextured_triangle_count
                = (g_battle_map_primary_untextured_triangle_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].untextured_quad_count = (g_battle_map_primary_untextured_quad_count - 1) / 8 + 1;
            g_battle_map_mesh_parts[i].textured_triangle_start
                = ((g_battle_map_primary_textured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].textured_quad_start
                = ((g_battle_map_primary_textured_quad_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].untextured_triangle_start
                = ((g_battle_map_primary_untextured_triangle_count - 1) / 8 + 1) * (i - 1);
            g_battle_map_mesh_parts[i].untextured_quad_start
                = ((g_battle_map_primary_untextured_quad_count - 1) / 8 + 1) * (i - 1);
        }
        g_battle_map_mesh_parts[0].textured_triangle_count = 0;
        g_battle_map_mesh_parts[0].textured_quad_count = 0;
        g_battle_map_mesh_parts[0].untextured_triangle_count = 0;
        g_battle_map_mesh_parts[0].untextured_quad_count = 0;
        g_battle_map_mesh_parts[0].textured_triangle_start = 0;
        g_battle_map_mesh_parts[0].textured_quad_start = 0;
        g_battle_map_mesh_parts[0].untextured_triangle_start = 0;
        g_battle_map_mesh_parts[0].untextured_quad_start = 0;
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
            g_battle_map_saved_texture_animation_modes[i] = g_battle_map_texture_animations[i].mode;
            g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_battle_map_texture_animations[i].mode = g_battle_map_saved_texture_animation_modes[i];
        }
        break;
    case MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_battle_map_saved_texture_animation_modes[index] = g_battle_map_texture_animations[index].mode;
            g_battle_map_texture_animations[index].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_battle_map_texture_animations[index].mode = g_battle_map_saved_texture_animation_modes[index];
        }
        break;
    case MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_battle_map_texture_animations[index].duration = object_state;
            g_battle_map_texture_animations[index].timer = 0;
        }
        break;
    case MAP_DATA_COMMAND_RELEASE_GNS_HOLD:
        g_battle_map_gns_dispatch_held = 0;
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
static void battle_map_update_texture_animations(void) {
    s32 i;

    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        if (g_battle_map_texture_animations[i].active == 0) {
            continue;
        }
        switch (g_battle_map_texture_animations[i].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP:
            g_battle_map_texture_animations[i].timer++;
            if (g_battle_map_texture_animations[i].timer < g_battle_map_texture_animations[i].duration) {
                break;
            }
            g_battle_map_texture_animations[i].timer = 0;
            g_battle_map_texture_animations[i].frame++;
            g_battle_map_texture_animations[i].column++;
            if (g_battle_map_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row++;
                g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2
                    - g_battle_map_texture_animations[i].width;
            }
            if (g_battle_map_texture_animations[i].frame_count - 1 < g_battle_map_texture_animations[i].frame) {
                g_battle_map_texture_animations[i].frame = 0;
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row = 0;
                g_battle_map_texture_animation_moves[i].sx
                    = g_battle_map_texture_animations[i].frame_x - g_battle_map_texture_animations[i].width;
            }
            g_battle_map_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
            g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
            AddPrim(g_battle_gfx_current_render_buffer->otag, &g_battle_map_texture_animation_moves[i]);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG:
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64
                        + 2 - g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame_count - 2 < g_battle_map_texture_animations[i].frame) {
                    g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK;
                }
                g_battle_map_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK:
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                if (g_battle_map_texture_animation_moves[i].sx - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (g_battle_map_texture_animations[i].frame_x % 64))
                        / g_battle_map_texture_animations[i].width;
                    g_battle_map_texture_animations[i].row--;
                    g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x
                        + g_battle_map_texture_animations[i].column * g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG;
                }
                g_battle_map_texture_animation_moves[i].sx -= g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP:
            g_battle_map_texture_animations[i].timer++;
            if (g_battle_map_texture_animations[i].timer < g_battle_map_texture_animations[i].duration) {
                break;
            }
            g_battle_map_texture_animations[i].timer = 0;
            g_battle_map_texture_animations[i].frame++;
            g_battle_map_texture_animations[i].column++;
            if (g_battle_map_texture_animations[i].frame_count - 1 < g_battle_map_texture_animations[i].frame) {
                g_battle_map_texture_animations[i].frame = 0;
                g_battle_map_texture_animations[i].column = 0;
            }
            battle_map_load_palette_data(g_battle_map_palette_animation_frames
                    + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].column,
                MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG:
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count - 1) {
                    g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK;
                }
                battle_map_load_palette_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK:
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG;
                }
                battle_map_load_palette_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
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
            g_battle_map_texture_animations[i].frame = 0;
            while (g_battle_map_texture_animations[i].frame < g_battle_map_texture_animations[i].frame_count) {
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64
                        + 2 - g_battle_map_texture_animations[i].width;
                }
                g_battle_map_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                g_battle_map_texture_animations[i].frame++;
            }
            g_battle_map_texture_animations[i].timer = 0x7E;
            g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING;
            /* fall through */
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                if (g_battle_map_texture_animation_moves[i].sx - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (g_battle_map_texture_animations[i].frame_x % 64))
                            / g_battle_map_texture_animations[i].width
                        - 1;
                    g_battle_map_texture_animations[i].row--;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].active = 0;
                    g_battle_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0;
                }
                g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x
                    + g_battle_map_texture_animations[i].width * g_battle_map_texture_animations[i].column;
                g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_battle_map_texture_animation_moves[i]);
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
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                if (g_battle_map_texture_animations[i].column != 0) {
                    g_battle_map_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                }
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].sx
                        = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    g_battle_map_texture_animation_moves[i].sy
                        += g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                }
                if (g_battle_map_texture_animations[i].frame > g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].frame = 0;
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row = 0;
                    g_battle_map_texture_animations[i].active = 0;
                    g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x;
                    g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_battle_map_texture_animation_moves[i]);
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
            g_battle_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                battle_map_load_palette_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].active = 0;
                }
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
            if (g_battle_map_texture_animation_cycle_handles[i] == 0) {
                g_battle_map_texture_animation_cycle_handles[i]
                    = battle_map_start_clut_cycle(g_battle_map_texture_animations[i].duration, MAP_CLUT_ROW_MAP,
                        g_battle_map_texture_animations[i].canvas_x,
                        g_battle_map_texture_animations[i].canvas_x + g_battle_map_texture_animations[i].width - 1);
                if (g_battle_map_texture_animation_cycle_handles[i] == 0) {
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
static map_state_record_t* battle_map_load_saved_state_record(map_state_record_t* record) {
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
            g_battle_map_mesh_animation_keyframes[i] = *src++;
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
                    g_battle_map_mesh_animation_instructions[i].states[y][c] = *src++;
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
            g_battle_map_mesh_parts[i].parent = *src;
            g_battle_map_mesh_parts[i].duration = 0;
            g_battle_map_mesh_parts[i].elapsed = 2;
            src += 4;
            if (g_battle_map_mesh_animation_instructions[i].states[0][1].next != 0) {
                g_battle_map_mesh_part_animation_states[i] = 1;
            } else {
                g_battle_map_mesh_part_animation_states[i] = 0;
            }
            g_battle_map_mesh_parts[i].keyframe = 0;
            g_battle_map_mesh_parts[i].rotation
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .rotation;
            g_battle_map_mesh_parts[i].translation
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .translation;
            g_battle_map_mesh_parts[i].scale.vx
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .scale_x;
            g_battle_map_mesh_parts[i].scale.vy
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .scale_y;
            g_battle_map_mesh_parts[i].scale.vz
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .scale_z;
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

    zero_vector = g_battle_map_zero_svector;
    unit_scale = g_battle_map_unit_scale_vector;
    while (DrawSync(1) != 0) { }
    switch (resource_type) {
    case MAP_RESOURCE_GEOMETRY_PRIMARY:
        g_battle_map_textured_triangle_count = 0;
        g_battle_map_textured_quad_count = 0;
        g_battle_map_untextured_triangle_count = 0;
        g_battle_map_untextured_quad_count = 0;
        for (i = 0; i < 9; i++) {
            g_battle_map_mesh_parts[i].textured_triangle_count = 0;
            g_battle_map_mesh_parts[i].textured_quad_count = 0;
            g_battle_map_mesh_parts[i].untextured_triangle_count = 0;
            g_battle_map_mesh_parts[i].untextured_quad_count = 0;
            g_battle_map_mesh_part_geometry[i] = 0;
        }
        g_battle_map_mesh_part_geometry[0] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[0]);
        g_battle_map_primary_textured_triangle_count = g_battle_map_textured_triangle_count;
        g_battle_map_primary_textured_quad_count = g_battle_map_textured_quad_count;
        g_battle_map_primary_untextured_triangle_count = g_battle_map_untextured_triangle_count;
        g_battle_map_primary_untextured_quad_count = g_battle_map_untextured_quad_count;
        break;
    case MAP_RESOURCE_GEOMETRY_PART_1:
        g_battle_map_mesh_part_geometry[1] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[1]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_2:
        g_battle_map_mesh_part_geometry[2] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[2]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_3:
        g_battle_map_mesh_part_geometry[3] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[3]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_4:
        g_battle_map_mesh_part_geometry[4] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[4]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_5:
        g_battle_map_mesh_part_geometry[5] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[5]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_6:
        g_battle_map_mesh_part_geometry[6] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[6]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_7:
        g_battle_map_mesh_part_geometry[7] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[7]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_8:
        g_battle_map_mesh_part_geometry[8] = &data->geometry;
        battle_map_append_mesh_geometry(&data->geometry, &g_battle_map_mesh_parts[8]);
        break;
    case MAP_RESOURCE_POLYGON_FLAGS:
        polygon_flags = data->polygon_flags.textured_triangles;
        for (i = 0; i < 360; i++) {
            flags = *polygon_flags++;
            g_battle_map_textured_triangle_positions[i].polygon_flags = flags;
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
            g_battle_map_textured_quad_positions[i].polygon_flags = flags;
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
            g_battle_map_untextured_triangle_positions[i].polygon_flags = *polygon_flags++;
        }
        for (i = 0; i < 256; i++) {
            g_battle_map_untextured_quad_positions[i].polygon_flags = *polygon_flags++;
        }
        battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        break;
    case MAP_RESOURCE_PALETTES:
        g_battle_map_palette_pending = 1;
        for (i = 0; i < 0x200; i++) {
            g_battle_map_pending_palette_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_PALETTES:
        g_battle_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MAP;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_palette_pending = 0;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_load_palette_data(data->clut, MAP_CLUT_ROW_MAP, 0, 1);
        break;
    case MAP_RESOURCE_MARKED_TILE_PALETTES:
        g_battle_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MARKED_TILES;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_load_palette_data(data->clut, MAP_CLUT_ROW_MARKED_TILES, 0, 1);
        break;
    case MAP_RESOURCE_UNSUPPORTED_12:
        main_system_handle_malloc_exception(2, 0x3ca);
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_0:
        g_battle_map_vram_load_rect.x = 0x300;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_1:
        g_battle_map_vram_load_rect.x = 0x340;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_2:
        g_battle_map_vram_load_rect.x = 0x380;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_3:
        g_battle_map_vram_load_rect.x = 0x3c0;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case MAP_RESOURCE_TEXTURE:
        if (g_battle_map_texture_quarter_0_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_0, data);
        }
        if (g_battle_map_texture_quarter_1_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_1, (map_resource_t*)(data->bytes + 0x8000));
        }
        if (g_battle_map_texture_quarter_2_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_2, (map_resource_t*)(data->bytes + 0x10000));
        }
        if (g_battle_map_texture_quarter_3_pending != 0) {
            battle_map_dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_3, (map_resource_t*)(data->bytes + 0x18000));
        }
        g_battle_map_texture_quarter_0_pending = 0;
        g_battle_map_texture_quarter_1_pending = 0;
        g_battle_map_texture_quarter_2_pending = 0;
        g_battle_map_texture_quarter_3_pending = 0;
        g_battle_map_texture_pending = 0;
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
        g_battle_map_lighting_pending = 1;
        for (i = 0; i < 0x2d; i++) {
            g_battle_map_pending_lighting_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_LIGHTING:
        light = &data->lighting;
        g_battle_map_lighting_pending = 0;
        setVector(&g_battle_map_light_rotation, 0, 0, 0);
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
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r = red;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g = green;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b = blue;
        red = light->gradient_bottom.r;
        green = light->gradient_bottom.g;
        blue = light->gradient_bottom.b;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = red;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = green;
        g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = blue;
        g_battle_map_lightning_state.base_colors[0]
            = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_battle_map_lightning_state.base_colors[1]
            = g_battle_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        battle_map_light_state_command(MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
        break;
    case MAP_RESOURCE_TEXTURE_ANIMATIONS:
        source = data->texture_animations;
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_battle_map_texture_animation_cycle_handles[i] = 0;
            g_battle_map_texture_animations[i] = source[i];
            SetDrawMove(&g_battle_map_texture_animation_moves[i]);
            g_battle_map_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x;
            g_battle_map_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y;
            g_battle_map_texture_animation_moves[i].w = g_battle_map_texture_animations[i].width;
            g_battle_map_texture_animation_moves[i].h = g_battle_map_texture_animations[i].height;
            g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].canvas_x;
            g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].canvas_y;
            if (g_battle_map_texture_animations[i].mode >= MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0
                && g_battle_map_texture_animations[i].mode < MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE) {
                g_battle_map_texture_animations[i].active = 0;
            } else if (g_battle_map_texture_animations[i].mode != MAP_TEXTURE_ANIMATION_MODE_NONE) {
                g_battle_map_texture_animations[i].active = 1;
            }
        }
        break;
    case MAP_RESOURCE_PALETTE_ANIMATION_FRAMES:
        for (i = 0; i < 0x100; i++) {
            /* Palette-bank indexing changes the native 16-bit copy loop. */
            ((u16*)g_battle_map_palette_animation_frames)[i] = ((u16*)data->clut)[i];
        }
        break;
    case MAP_RESOURCE_TERRAIN: {
        map_terrain_resource_t* terrain = &data->terrain;
        g_battle_map_tile_width = terrain->width;
        g_battle_map_tile_depth = terrain->depth;
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
        setVector(&g_battle_camera_integer_coords, -vector[0], vector[1], -vector[2]);
        battle_camera_set_focus_and_cursor_tile(&g_battle_camera_integer_coords, &g_battle_map_camera_rotation);
        break;
    case GNS_COMMAND_SET_CAMERA_ROTATION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_battle_map_camera_rotation, vector[0], vector[1], vector[2]);
        battle_camera_set_focus_and_cursor_tile(&g_battle_camera_integer_coords, &g_battle_map_camera_rotation);
        break;
    /* Case-local pointers preserve the native address lifetimes. */
    case GNS_COMMAND_SET_INDOOR: {
        s32* weather_flags;

        weather_flags = &g_battle_map_weather_flags;
        *weather_flags |= MAP_WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    }
    case GNS_COMMAND_SET_OUTDOOR: {
        s32* weather_flags;

        weather_flags = &g_battle_map_weather_flags;
        *weather_flags &= ~MAP_WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_battle_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_battle_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    }
    case GNS_COMMAND_CLEAR_SNOW: {
        s32* weather_flags;

        weather_flags = &g_battle_map_weather_flags;
        *weather_flags &= ~MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_SNOW: {
        s32* weather_flags;

        weather_flags = &g_battle_map_weather_flags;
        *weather_flags |= MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_WEATHER_MODIFIER: {
        s32* weather_flags;

        vector = data->command_halfwords;
        vector += 6;
        weather_flags = &g_battle_map_weather_flags;
        *weather_flags = (*weather_flags & MAP_WEATHER_LOW_FLAGS_MASK) | (vector[0] << MAP_WEATHER_MODIFIER_SHIFT);
        break;
    }
    }
    return result;
}

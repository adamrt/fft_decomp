/* Map polygon flags and lighting. Its start is a boundary the binary proves; its end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Textured polygon positions (map_append_mesh_geometry). polygon_flags holds
 * the polygon flags loaded by GNS resource 0x2c (map_dispatch_gns_resource). */

/*
 * Map polygon flag commands for the textured triangles and quads.
 *
 * 0x94 restores the ambient color g_map_ambient_polygon_color on
 * every polygon whose flag bit 15 is clear and records the render buffer it
 * wrote in g_map_ambient_restored_buffer; the map draw loop repeats the restore on the other
 * buffer and clears it. 0x46 copies flag bit 15 into bit 0 (after the flags
 * load and on the tile-color reset); 0x47 clears bit 0 everywhere and stops
 * the g_map_polygon_flag_clear_countdown countdown whose expiry issues it. The flag field is s16
 * (the bit 15 tests load it with `lh`).
 */
void battle_map_polygon_flag_command(s32 command) {
    s32 i;

    switch (command) {
    case MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_map_ambient_polygon_color.b;
            }
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_map_ambient_polygon_color.b;
                (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_map_ambient_polygon_color.r;
                (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_map_ambient_polygon_color.g;
                (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_map_ambient_polygon_color.b;
            }
        }
        g_map_ambient_restored_buffer = g_battle_gfx_current_render_buffer;
        break;
    case MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        break;
    case MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        g_map_polygon_flag_clear_countdown = 0;
        break;
    }
}

/* Main-executable libgte routine at 0x8001d108; it loads the matrix into GTE
 * control registers 16-20 (the color matrix), like Psy-Q SetColorMatrix. */

/*
 * Copy two matrices into the working pair at g_map_light_color_matrix and into entries 0 and
 * 2 of g_map_lightning_state.color_matrices, then load the first into the GTE color matrix.
 *
 * Each backup copy rereads the working field; a chained assignment assigns the
 * loop pointers to different registers.
 */
void battle_map_set_light_matrices(MATRIX* first, MATRIX* second) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_map_light_color_matrix.m[i][j] = first->m[i][j];
            g_map_lightning_state.color_matrices[0].m[i][j] = g_map_light_color_matrix.m[i][j];
        }
        g_map_light_color_matrix.t[i] = first->t[i];
        g_map_lightning_state.color_matrices[0].t[i] = g_map_light_color_matrix.t[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_map_light_matrix.m[i][j] = second->m[i][j];
            g_map_lightning_state.color_matrices[2].m[i][j] = g_map_light_matrix.m[i][j];
        }
        g_map_light_matrix.t[i] = second->t[i];
        g_map_lightning_state.color_matrices[2].t[i] = g_map_light_matrix.t[i];
    }
    SetColorMatrix(&g_map_light_color_matrix);
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

        if (g_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_map_back_color_red;
        *stored_values = value_1;
        g_map_back_color_green = value_2;
        g_map_back_color_blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case BATTLE_BACKGROUND_COLOR_GET:
        result = &g_map_back_color_red;
        break;
    case BATTLE_BACKGROUND_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_map_back_color_disabled = 1;
        return result;
    case BATTLE_BACKGROUND_COLOR_RESTORE:
        SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        g_map_back_color_disabled = 0;
        return result;
    }
    return result;
}

/*
 * Map light matrix commands, selected on command - 0x5e: 0x5e loads both
 * matrices from the arguments and applies the color matrix to the GTE, 0x5f
 * saves and 0x60 restores them, 0x61 and 0x62 return the color and light
 * matrices. 0x66 and 0x68 start a transition from the current matrices over
 * duration frames and queue 0x67 or 0x69 in g_map_light_transition_command, which the map draw
 * loop then issues once per frame: 0x67 interpolates toward target matrices,
 * 0x69 adds a frame-scaled delta.
 *
 * The translation rows add the start matrix element m[i][3] (the loop index
 * after the inner loop) rather than the start translation, as in the target.
 * Commands 0x5e-0x60 and 0x66-0x69 return the uninitialized result, like
 * map_light_state_command.
 */
MATRIX* battle_map_light_matrix_command(
    map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration) {
    MATRIX* result;
    s32 i;
    s32 j;

    switch (command) {
    case MAP_LIGHT_MATRIX_COMMAND_SET:
        g_map_light_color_matrix = *color;
        g_map_light_matrix = *light;
        SetColorMatrix(&g_map_light_color_matrix);
        break;
    case MAP_LIGHT_MATRIX_COMMAND_SAVE:
        g_map_saved_light_color_matrix = g_map_light_color_matrix;
        g_map_saved_light_matrix = g_map_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_RESTORE:
        g_map_light_color_matrix = g_map_saved_light_color_matrix;
        g_map_light_matrix = g_map_saved_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_GET_COLOR:
        result = &g_map_light_color_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT:
        result = &g_map_light_matrix;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_START_BLEND:
        g_map_light_transition_start_color_matrix = g_map_light_color_matrix;
        g_map_light_transition_start_light_matrix = g_map_light_matrix;
        g_map_light_transition_target_color_matrix = *color;
        g_map_light_transition_target_light_matrix = *light;
        g_map_light_transition_frame = 0;
        g_map_light_transition_duration = duration;
        g_map_light_transition_command = MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_map_light_color_matrix.m[i][j] = g_map_light_transition_start_color_matrix.m[i][j]
                    + (g_map_light_transition_target_color_matrix.m[i][j]
                          - g_map_light_transition_start_color_matrix.m[i][j])
                        * g_map_light_transition_frame / g_map_light_transition_duration;
                g_map_light_matrix.m[i][j] = g_map_light_transition_start_light_matrix.m[i][j]
                    + (g_map_light_transition_target_light_matrix.m[i][j]
                          - g_map_light_transition_start_light_matrix.m[i][j])
                        * g_map_light_transition_frame / g_map_light_transition_duration;
            }
            g_map_light_color_matrix.t[i] = g_map_light_transition_start_color_matrix.m[i][j]
                + (g_map_light_transition_target_color_matrix.t[i] - g_map_light_transition_start_color_matrix.t[i])
                    * g_map_light_transition_frame / g_map_light_transition_duration;
            g_map_light_matrix.t[i] = g_map_light_transition_start_light_matrix.m[i][j]
                + (g_map_light_transition_target_light_matrix.t[i] - g_map_light_transition_start_light_matrix.t[i])
                    * g_map_light_transition_frame / g_map_light_transition_duration;
        }
        SetColorMatrix(&g_map_light_color_matrix);
        g_map_light_transition_frame++;
        if (g_map_light_transition_frame > g_map_light_transition_duration) {
            g_map_light_transition_command = 0;
        }
        break;
    case MAP_LIGHT_MATRIX_COMMAND_START_DELTA:
        g_map_light_transition_start_color_matrix = g_map_light_color_matrix;
        g_map_light_transition_start_light_matrix = g_map_light_matrix;
        g_map_light_transition_target_color_matrix = *color;
        g_map_light_transition_target_light_matrix = *light;
        g_map_light_transition_frame = 0;
        g_map_light_transition_duration = duration;
        g_map_light_transition_command = MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA;
        break;
    case MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA:
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                g_map_light_color_matrix.m[i][j] = g_map_light_transition_start_color_matrix.m[i][j]
                    + g_map_light_transition_target_color_matrix.m[i][j] * g_map_light_transition_frame
                        / g_map_light_transition_duration;
                g_map_light_matrix.m[i][j] = g_map_light_transition_start_light_matrix.m[i][j]
                    + g_map_light_transition_target_light_matrix.m[i][j] * g_map_light_transition_frame
                        / g_map_light_transition_duration;
            }
            g_map_light_color_matrix.t[i] = g_map_light_transition_start_color_matrix.m[i][j]
                + g_map_light_transition_frame * g_map_light_transition_target_color_matrix.t[i]
                    / g_map_light_transition_duration;
            g_map_light_matrix.t[i] = g_map_light_transition_start_light_matrix.m[i][j]
                + g_map_light_transition_frame * g_map_light_transition_target_light_matrix.t[i]
                    / g_map_light_transition_duration;
        }
        SetColorMatrix(&g_map_light_color_matrix);
        g_map_light_transition_frame++;
        if (g_map_light_transition_frame > g_map_light_transition_duration) {
            g_map_light_transition_command = 0;
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
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP]
            = ((map_background_gradient_colors_t*)data)->colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM]
            = ((map_background_gradient_colors_t*)data)->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT:
        result = &g_map_background_gradient_colors;
        break;
    case MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT:
        g_map_saved_background_gradient_first = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP];
        g_map_saved_background_gradient_second
            = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT:
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP] = g_map_saved_background_gradient_first;
        g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM]
            = g_map_saved_background_gradient_second;
        break;
    case MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR:
        g_map_back_color_red = ((CVECTOR*)data)->r;
        g_map_back_color_green = ((CVECTOR*)data)->g;
        g_map_back_color_blue = ((CVECTOR*)data)->b;
        SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        break;
    case MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR:
        g_map_saved_back_color_red = g_map_back_color_red;
        g_map_saved_back_color_green = g_map_back_color_green;
        g_map_saved_back_color_blue = g_map_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR:
        g_map_back_color_red = g_map_saved_back_color_red;
        g_map_back_color_green = g_map_saved_back_color_green;
        g_map_back_color_blue = g_map_saved_back_color_blue;
        break;
    case MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR: {
        CVECTOR* color = &g_map_cached_back_color;

        color->r = g_map_back_color_red;
        g_map_cached_back_color.g = g_map_back_color_green;
        g_map_cached_back_color.b = g_map_back_color_blue;
        result = color;
        break;
    }
    case MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR:
        g_map_current_darkness_color.r = ((CVECTOR*)data)->r;
        g_map_current_darkness_color.g = ((CVECTOR*)data)->g;
        g_map_current_darkness_color.b = ((CVECTOR*)data)->b;
        break;
    case MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR:
        g_map_current_darkness_color.r = 0;
        g_map_current_darkness_color.g = 0;
        g_map_current_darkness_color.b = 1;
        break;
    case MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR:
        result = &g_map_current_darkness_color;
        break;
    }
    return result;
}

#define GT3(i)       (g_battle_gfx_current_render_buffer->gt3[i])
#define GT4(i)       (g_battle_gfx_current_render_buffer->gt4[i])
#define SAVED_GT3(i) (g_target_tile_color_buffer->gt3[i])
#define SAVED_GT4(i) g_target_tile_color_buffer->gt4[i]

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

    memcpy(splash_uv, g_map_weather_splash_uv_rects, sizeof(splash_uv));
    memcpy(drop_uv, g_map_weather_drop_uv_rects, sizeof(drop_uv));
    unused = g_map_white_cvector;
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay_modes[1]);
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay);
    AddPrim(g_battle_gfx_current_render_buffer->otag + 0x17F, &g_battle_gfx_current_render_buffer->overlay_modes[0]);
    battle_map_update_texture_animations();
    battle_map_update_mesh_part_animations();
    if (g_map_lightning_mode != MAP_LIGHTNING_OFF && g_sound_weather_sfx_enabled != 0) {
        if (rand() % 480 == 0) {
            g_map_lightning_state.timer = 60;
            if (rand() & 1) {
                g_sound_secondary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_A);
                main_sound_play_sfx_find_channel(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_A));
            } else {
                g_sound_tertiary_weather_sfx_id = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_B);
                main_sound_play_sfx_find_channel(SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, MAIN_ENV_SFX_THUNDER_B));
            }
        }
        if (g_map_lightning_state.timer != 0) {
            u32 remaining;

            remaining = --g_map_lightning_state.timer;
            g_map_lightning_state.colors[0].r = g_map_lightning_state.base_colors[0].r
                - g_map_lightning_state.base_colors[0].r * remaining / 60 + remaining * 208 / 60;
            g_map_lightning_state.colors[0].g = g_map_lightning_state.base_colors[0].g
                - g_map_lightning_state.base_colors[0].g * remaining / 60 + remaining * 208 / 60;
            g_map_lightning_state.colors[0].b = g_map_lightning_state.base_colors[0].b
                - g_map_lightning_state.base_colors[0].b * remaining / 60 + remaining * 3;
            g_map_lightning_state.colors[1].r = g_map_lightning_state.base_colors[1].r
                - g_map_lightning_state.base_colors[1].r * remaining / 60 + remaining * 112 / 60;
            g_map_lightning_state.colors[1].g = g_map_lightning_state.base_colors[1].g
                - g_map_lightning_state.base_colors[1].g * remaining / 60 + remaining * 112 / 60;
            g_map_lightning_state.colors[1].b = g_map_lightning_state.base_colors[1].b
                - g_map_lightning_state.base_colors[1].b * remaining / 60 + remaining * 96 / 60;
            battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &g_map_lightning_state.colors[0]);
        } else {
            battle_map_light_state_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
            if (g_map_lightning_mode == MAP_LIGHTNING_ONCE) {
                g_map_lightning_mode = MAP_LIGHTNING_OFF;
            }
        }
        if (g_map_lightning_state.timer == 0x3A || g_map_lightning_state.timer == 0x38) {
            SetColorMatrix(&g_map_lightning_state.color_matrices[1]);
            SetBackColor(0, 0, 0x40);
        }
        if (g_map_lightning_state.timer == 0x39 || g_map_lightning_state.timer == 0x33) {
            SetColorMatrix(&g_map_lightning_state.color_matrices[0]);
            SetBackColor(g_map_back_color_red, g_map_back_color_green, g_map_back_color_blue);
        }
    }
    if (g_map_light_transition_command != 0) {
        battle_map_light_matrix_command(
            g_map_light_transition_command, &g_map_light_color_matrix, &g_map_light_matrix, 0);
    }
    g_battle_gfx_current_render_buffer->overlay.r0
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_battle_gfx_current_render_buffer->overlay.g0
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_battle_gfx_current_render_buffer->overlay.b0
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_battle_gfx_current_render_buffer->overlay.r1
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_battle_gfx_current_render_buffer->overlay.g1
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_battle_gfx_current_render_buffer->overlay.b1
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_battle_gfx_current_render_buffer->overlay.r2
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_battle_gfx_current_render_buffer->overlay.g2
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_battle_gfx_current_render_buffer->overlay.b2
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    g_battle_gfx_current_render_buffer->overlay.r3
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_battle_gfx_current_render_buffer->overlay.g3
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_battle_gfx_current_render_buffer->overlay.b3
        = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    MulMatrix0(&g_map_light_matrix, &g_map_light_rotation_matrix, &g_map_local_light_matrix);
    SetLightMatrix(&g_map_local_light_matrix);
    if (g_map_polygon_flag_clear_countdown != 0) {
        if (--g_map_polygon_flag_clear_countdown == 0) {
            battle_map_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE);
        }
    }
    SetRotMatrix(camera);
    SetTransMatrix(camera);
    g_map_part_textured_triangle_count = g_map_mesh_parts[0].textured_triangle_count;
    g_map_part_textured_triangle_positions
        = &g_map_textured_triangle_positions[g_map_mesh_parts[0].textured_triangle_start];
    g_map_part_textured_triangle_normals
        = &g_map_textured_triangle_normals[g_map_mesh_parts[0].textured_triangle_start];
    map_queue_textured_triangles(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->gt3[g_map_mesh_parts[0].textured_triangle_start], g_map_camera_rotation.vy,
        &i);
    g_map_part_textured_quad_count = g_map_mesh_parts[0].textured_quad_count;
    g_map_part_textured_quad_positions = &g_map_textured_quad_positions[g_map_mesh_parts[0].textured_quad_start];
    g_map_part_textured_quad_normals = &g_map_textured_quad_normals[g_map_mesh_parts[0].textured_quad_start];
    map_queue_textured_quads(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->gt4[g_map_mesh_parts[0].textured_quad_start], g_map_camera_rotation.vy,
        &i);
    g_map_part_untextured_triangle_count = g_map_mesh_parts[0].untextured_triangle_count;
    g_map_part_untextured_triangle_positions
        = &g_map_untextured_triangle_positions[g_map_mesh_parts[0].untextured_triangle_start];
    map_queue_untextured_triangles(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->f3[g_map_mesh_parts[0].untextured_triangle_start],
        g_map_camera_rotation.vy, &i);
    g_map_part_untextured_quad_count = g_map_mesh_parts[0].untextured_quad_count;
    g_map_part_untextured_quad_positions = &g_map_untextured_quad_positions[g_map_mesh_parts[0].untextured_quad_start];
    map_queue_untextured_quads(g_battle_gfx_current_render_buffer->otag,
        &g_battle_gfx_current_render_buffer->f4[g_map_mesh_parts[0].untextured_quad_start], g_map_camera_rotation.vy,
        &i);
    /* Dead store: `matrix` is reassigned for every drawn child part before
     * use. It still survives the first CSE pass (the loop uses `matrix`), and
     * the second pass reuses it for the loop-hoisted &g_map_mesh_parts[0].matrix
     * constant; reload then deletes that spilled constant together with this
     * set. The transient local moves the draw calls' `&i` temporary from v0
     * to a0, as in the target. Earlier than the textured_triangle_count access, it would
     * become the base of that address instead. */
    matrix = &g_map_mesh_parts[0].matrix;
    PushMatrix();
    for (part = 1; part < 9; part++) {
        if (g_map_mesh_animation_instructions[part].states[0][1].next != 0) {
            u8 parent;

            rotation = &g_map_mesh_parts[part].rotation;
            matrix = &g_map_mesh_parts[part].matrix;
            parent = g_map_mesh_parts[part].parent;
            RotMatrix(rotation, matrix);
            MulMatrix2(&g_map_mesh_parts[parent].matrix, matrix);
            SetRotMatrix(&g_map_mesh_parts[parent].matrix);
            SetTransMatrix(&g_map_mesh_parts[parent].matrix);
            RotTrans(&g_map_mesh_parts[part].translation, (VECTOR*)matrix->t, &flag);
            ScaleMatrix(matrix, &g_map_mesh_parts[part].scale);
            RotMatrix(rotation, &light_rotation);
            MulMatrix0(&g_map_local_light_matrix, &light_rotation, &light_matrix);
            SetLightMatrix(&light_matrix);
            SetRotMatrix(matrix);
            SetTransMatrix(matrix);
            g_map_part_textured_triangle_count = g_map_mesh_parts[part].textured_triangle_count;
            g_map_part_textured_triangle_positions
                = &g_map_textured_triangle_positions[g_map_mesh_parts[part].textured_triangle_start];
            g_map_part_textured_triangle_normals
                = &g_map_textured_triangle_normals[g_map_mesh_parts[part].textured_triangle_start];
            map_queue_textured_triangles(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->gt3[g_map_mesh_parts[part].textured_triangle_start],
                g_map_camera_rotation.vy, &i);
            g_map_part_textured_quad_count = g_map_mesh_parts[part].textured_quad_count;
            g_map_part_textured_quad_positions
                = &g_map_textured_quad_positions[g_map_mesh_parts[part].textured_quad_start];
            g_map_part_textured_quad_normals = &g_map_textured_quad_normals[g_map_mesh_parts[part].textured_quad_start];
            map_queue_textured_quads(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->gt4[g_map_mesh_parts[part].textured_quad_start],
                g_map_camera_rotation.vy, &i);
            g_map_part_untextured_triangle_count = g_map_mesh_parts[part].untextured_triangle_count;
            g_map_part_untextured_triangle_positions
                = &g_map_untextured_triangle_positions[g_map_mesh_parts[part].untextured_triangle_start];
            map_queue_untextured_triangles(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->f3[g_map_mesh_parts[part].untextured_triangle_start],
                g_map_camera_rotation.vy, &i);
            g_map_part_untextured_quad_count = g_map_mesh_parts[part].untextured_quad_count;
            g_map_part_untextured_quad_positions
                = &g_map_untextured_quad_positions[g_map_mesh_parts[part].untextured_quad_start];
            map_queue_untextured_quads(g_battle_gfx_current_render_buffer->otag,
                &g_battle_gfx_current_render_buffer->f4[g_map_mesh_parts[part].untextured_quad_start],
                g_map_camera_rotation.vy, &i);
            if (g_map_untextured_otz_mask != 0 && g_effect_map_vertices != 0
                && (g_map_loaded_id == MAP_ID_ENTRANCE_TO_DEATH_CITY || g_map_loaded_id == MAP_ID_GRAVEYARD_OF_AIRSHIPS
                    || g_map_loaded_id == MAP_ID_ZIREKILE_FALLS)) {
                RotMatrix(&g_map_mesh_parts[part].rotation, &effect_matrix);
                effect_translation.vx = g_map_mesh_parts[part].translation.vx;
                effect_translation.vy = g_map_mesh_parts[part].translation.vy;
                effect_translation.vz = g_map_mesh_parts[part].translation.vz;
                TransMatrix(&effect_matrix, &effect_translation);
                ScaleMatrix(&effect_matrix, &g_map_mesh_parts[part].scale);
                SetRotMatrix(&effect_matrix);
                SetTransMatrix(&effect_matrix);
                for (j = 0; j < 4; j++) {
                    effect_base = (part - 1) * 4 + 0x61;
                    RotTrans(&g_map_mesh_animation_keyframes[j + effect_base].translation,
                        g_effect_map_vertices + j + (part - 1) * 4, &effect_flag);
                }
            }
        }
    }
    PopMatrix();
    if (g_map_weather_effect_mode == MAP_WEATHER_EFFECT_SNOW) {
        PushMatrix();
        g_map_weather_rotation.vy += g_map_weather_primary_speed;
        RotMatrix(&g_map_weather_rotation, &g_map_weather_matrix);
        TransMatrix(&g_map_weather_matrix, &g_map_weather_translation);
        ScaleMatrix(&g_map_weather_matrix, &g_map_weather_scale);
        SetRotMatrix(&g_map_weather_matrix);
        SetTransMatrix(&g_map_weather_matrix);
        half_size = g_map_camera_zoom.vx * g_map_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_map_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
            if (g_battle_render_buffers[0].ft3[i].y0 >= 0x119) {
                if (g_map_weather_scale.vy != 0) {
                    g_map_weather_particles[i].y = -0x100000 / g_map_weather_scale.vy;
                }
                g_map_weather_particles[i].fall_speed = 0;
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
                g_map_weather_particles[i].fall_speed = sprite + g_map_weather_fall_speed;
                if (g_map_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_map_weather_particles[i].x = rand() % (0x8C000 / g_map_weather_scale.vx);
                    } else {
                        g_map_weather_particles[i].x = -(rand() % (0x8C000 / g_map_weather_scale.vx));
                    }
                }
                if (g_map_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_map_weather_particles[i].z = rand() % (0x8C000 / g_map_weather_scale.vz);
                    } else {
                        g_map_weather_particles[i].z = -(rand() % (0x8C000 / g_map_weather_scale.vz));
                    }
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_map_weather_particles[i].x = g_map_weather_particles[i].x + 1;
                } else {
                    g_map_weather_particles[i].x = g_map_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_map_weather_particles[i].z = g_map_weather_particles[i].z + 1;
                } else {
                    g_map_weather_particles[i].z = g_map_weather_particles[i].z - 1;
                }
            }
            g_map_weather_particles[i].y += g_map_weather_particles[i].fall_speed * g_main_system_animation_speed;
            RotTrans((SVECTOR*)&g_map_weather_particles[i], &screen, &flag);
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
    if (g_map_weather_effect_mode == MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
        s32 sine;

        PushMatrix();
        g_map_command_0x96_rotation.vy = g_map_camera_rotation.vy;
        g_map_command_0x96_rotation.vz = -(rsin((g_map_command_0x96_frame << 10) / g_map_command_0x96_duration) >> 4);
        sine = rsin((g_map_command_0x96_frame << 10) / g_map_command_0x96_duration);
        g_map_weather_scale.vx = sine * 4 + ONE;
        g_map_weather_scale.vz = sine * 4 + ONE;
        g_map_weather_scale.vy = sine + ONE;
        g_map_command_0x96_frame++;
        RotMatrix(&g_map_command_0x96_rotation, &g_map_weather_matrix);
        TransMatrix(&g_map_weather_matrix, &g_map_weather_translation);
        ScaleMatrix(&g_map_weather_matrix, &g_map_weather_scale);
        SetRotMatrix(&g_map_weather_matrix);
        SetTransMatrix(&g_map_weather_matrix);
        half_size = g_map_camera_zoom.vx * g_map_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_map_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
            tile = (g_map_weather_particles[i + 32].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_map_weather_particles[i + 32].x / MAP_TILE_SIZE;
            ground = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
            if ((g_map_command_0x96_frame < g_map_command_0x96_duration / 8
                    && g_battle_render_buffers[0].ft3[i].y0 >= 0x119)
                || (g_map_command_0x96_duration / 8 < g_map_command_0x96_frame
                    && g_battle_render_buffers[0].ft3[i].y0 < 0)) {
                if (g_map_weather_scale.vy != 0) {
                    if (g_map_command_0x96_frame < g_map_command_0x96_duration / 8) {
                        g_map_weather_particles[i].y = -0x100000 / g_map_weather_scale.vy;
                    }
                    if (g_map_command_0x96_duration / 8 < g_map_command_0x96_frame) {
                        g_map_weather_particles[i].y = 0x30000 / g_map_weather_scale.vy;
                    }
                }
                sprite = rand() % 3;
                g_map_weather_particles[i].fall_speed = g_map_weather_fall_speed + 3 - sprite;

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
                if (g_map_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_map_weather_particles[i].x = rand() % (0x8C000 / g_map_weather_scale.vx);
                    } else {
                        g_map_weather_particles[i].x = -(rand() % (0x8C000 / g_map_weather_scale.vx));
                    }
                }
                if (g_map_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_map_weather_particles[i].z = rand() % (0x8C000 / g_map_weather_scale.vz);
                    } else {
                        g_map_weather_particles[i].z = -(rand() % (0x8C000 / g_map_weather_scale.vz));
                    }
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_map_weather_particles[i].x = g_map_weather_particles[i].x + 1;
                } else {
                    g_map_weather_particles[i].x = g_map_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_map_weather_particles[i].z = g_map_weather_particles[i].z + 1;
                } else {
                    g_map_weather_particles[i].z = g_map_weather_particles[i].z - 1;
                }
            }
            if (g_map_command_0x96_frame < g_map_command_0x96_duration / 8) {
                g_map_weather_particles[i].y += g_map_weather_particles[i].fall_speed * g_main_system_animation_speed
                    * rsin((g_map_command_0x96_frame << 13) / g_map_command_0x96_duration + 0x400) / 2048;
            }
            if (g_map_command_0x96_duration / 8 < g_map_command_0x96_frame) {
                g_map_weather_particles[i].y -= g_map_weather_particles[i].fall_speed * g_main_system_animation_speed;
            }
            RotTrans((SVECTOR*)&g_map_weather_particles[i], &screen, &flag);
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
        if (g_map_command_0x96_frame == g_map_command_0x96_duration) {
            battle_map_run_weather_command(MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
            g_map_weather_scale.vx = ONE;
            g_map_weather_scale.vy = ONE;
            g_map_weather_scale.vz = ONE;
            g_map_weather_rotation.vx = 0x10;
            g_map_weather_rotation.vz = 0x10;
            g_map_weather_translation.vx = 0x100;
            g_map_weather_translation.vy = 0xF0;
            g_map_weather_rotation.vy = 0;
            g_map_weather_translation.vz = 0x280;
        }
        PopMatrix();
    }
    if (g_map_weather_effect_mode == MAP_WEATHER_EFFECT_RAIN) {
        half_size = g_map_camera_zoom.vx * 22;
        half_size /= 0x2000;
        for (i = 0; i < 32; i++) {
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_map_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_map_weather_particles[i].x / MAP_TILE_SIZE;
            ground = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth)) * 12);
            if (ground < g_map_weather_particles[i + 32].y) {
                g_map_weather_particles[i].fall_speed = 0;
                g_map_weather_particles[i].x = rand() % (g_map_tile_width * MAP_TILE_SIZE);
                g_map_weather_particles[i].z = rand() % (g_map_tile_depth * MAP_TILE_SIZE);
                spawn_y = -0x180;
                g_map_weather_particles[i].y = spawn_y;
                g_map_weather_particles[i + 32].x = g_map_weather_particles[i].x;
                g_map_weather_particles[i + 32].z = g_map_weather_particles[i].z;
                g_map_weather_particles[i + 32].y
                    = spawn_y - g_map_weather_fall_speed - rand() % (g_map_weather_primary_speed * 12);
            }
            g_map_weather_particles[i].y += g_map_weather_fall_speed * g_main_system_animation_speed;
            g_map_weather_particles[i + 32].y += g_map_weather_fall_speed * g_main_system_animation_speed;
            if (i < 16) {
                g_map_weather_particles[i].y += g_map_weather_primary_speed * g_main_system_animation_speed;
                g_map_weather_particles[i + 32].y += g_map_weather_primary_speed * g_main_system_animation_speed;
            } else {
                g_map_weather_particles[i].y += g_map_rain_secondary_fall_speed * g_main_system_animation_speed;
                g_map_weather_particles[i + 32].y += g_map_rain_secondary_fall_speed * g_main_system_animation_speed;
            }
            if (g_map_weather_particles[i].y >= -0x17F) {
                if (ground < g_map_weather_particles[i].y) {
                    g_map_weather_particles[i].y = g_map_lowest_surface_y;
                } else if (g_map_weather_particles[i + 32].y < ground) {
                    RotTrans((SVECTOR*)&g_map_weather_particles[i], &screen, &flag);
                    sx = screen.vx;
                    sy = screen.vy;
                    g_battle_gfx_current_render_buffer->ft3[i].x2 = sx + 1;
                    g_battle_gfx_current_render_buffer->ft3[i].x0 = sx;
                    g_battle_gfx_current_render_buffer->ft3[i].y0 = sy;
                    g_battle_gfx_current_render_buffer->ft3[i].y2 = sy - 1;
                    RotTrans((SVECTOR*)&g_map_weather_particles[i + 32], &screen, &flag);
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
            if (g_map_weather_particles[i].fall_speed == 0
                && ((ground < g_map_weather_particles[i].y && g_map_weather_particles[i + 32].y < ground)
                    || g_map_weather_particles[i].y == g_map_lowest_surface_y)) {
                g_map_rain_splash_particles[i].fall_speed = 0x10;
                g_map_rain_splash_particles[i].y = ground - 8;
                g_map_rain_splash_particles[i].x = g_map_weather_particles[i].x;
                g_map_rain_splash_particles[i].z = g_map_weather_particles[i].z;
                if (!(g_map_tile_data_ptr[tile].flags.untargetable)) {
                    g_map_weather_particles[i].fall_speed = 1;
                } else {
                    g_map_weather_particles[i].fall_speed = 2;
                }
            }
            if (g_map_rain_splash_particles[i].fall_speed != 0) {
                RotTrans((SVECTOR*)&g_map_rain_splash_particles[i], &screen, &flag);
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
                    if (g_map_weather_particles[i].fall_speed == 1) {
                        AddPrim(g_battle_gfx_current_render_buffer->otag + depth,
                            &g_battle_gfx_current_render_buffer->ft3[i + 32]);
                    }
                }
                g_map_rain_splash_particles[i].fall_speed--;
                frame = g_map_rain_splash_particles[i].fall_speed;
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
                if (g_map_rain_splash_particles[i].fall_speed == 0) {
                    g_map_weather_particles[i].fall_speed = 0;
                }
            }
        }
    }
    /* The ambient color restores use pointer arithmetic on the decayed arrays
     * rather than GT3(i)/GT4(i) indexing: that keeps the target's
     * `addu offset, base` operand order (as in target_set_tile_background_color). */
    if (g_map_ambient_restored_buffer != 0) {
        if (g_map_ambient_restored_buffer != g_battle_gfx_current_render_buffer) {
            for (i = 0; i < g_map_textured_triangle_count; i++) {
                if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r0 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g0 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b0 = g_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r1 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g1 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b1 = g_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->r2 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->g2 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt3 + i)->b2 = g_map_ambient_polygon_color.b;
                }
            }
            for (i = 0; i < g_map_textured_quad_count; i++) {
                if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r0 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g0 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b0 = g_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r1 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g1 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b1 = g_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r2 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g2 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b2 = g_map_ambient_polygon_color.b;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->r3 = g_map_ambient_polygon_color.r;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->g3 = g_map_ambient_polygon_color.g;
                    (g_battle_gfx_current_render_buffer->gt4 + i)->b3 = g_map_ambient_polygon_color.b;
                }
            }
        }
        g_map_ambient_restored_buffer = 0;
    }
    g_map_tile_data_ptr = g_map_tile_data;
    if (g_target_tile_mark_modes[0] == BATTLE_TILE_MARK_SHOW_MOVE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
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
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
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
        g_target_tile_mark_modes[0] = BATTLE_TILE_MARK_MOVE_SHOWN;
    }
    if (g_target_tile_mark_modes[1] == BATTLE_TILE_MARK_SHOW_RANGE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
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
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
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
        g_target_tile_mark_modes[1] = BATTLE_TILE_MARK_RANGE_SHOWN;
    }
    if (g_target_tile_mark_modes[2] == BATTLE_TILE_MARK_SHOW_TARGETS) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
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
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_target_color_tile_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_target_color_tile_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_target_color_tile_y * g_map_tile_width + g_target_color_tile_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
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
        g_target_tile_mark_modes[2] = BATTLE_TILE_MARK_TARGETS_SHOWN;
    }
    if (g_target_tile_mark_modes[0] == BATTLE_TILE_MARK_HIDE_MOVE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
            if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                GT3(i).r0 = SAVED_GT3(i).r0;
                GT3(i).r1 = SAVED_GT3(i).r1;
                GT3(i).r2 = SAVED_GT3(i).r2;
            }
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
            if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                GT4(i).r0 = SAVED_GT4(i).r0;
                GT4(i).r1 = SAVED_GT4(i).r1;
                GT4(i).r2 = SAVED_GT4(i).r2;
                GT4(i).r3 = SAVED_GT4(i).r3;
            }
        }
        g_target_tile_mark_modes[0] = BATTLE_TILE_MARK_NONE;
        if ((g_target_tile_mark_modes[1] | g_target_tile_mark_modes[2]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_target_tile_mark_modes[1] == BATTLE_TILE_MARK_HIDE_RANGE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
        }
        g_target_tile_mark_modes[1] = BATTLE_TILE_MARK_NONE;
        if ((g_target_tile_mark_modes[0] | g_target_tile_mark_modes[2]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
    if (g_target_tile_mark_modes[2] == BATTLE_TILE_MARK_HIDE_TARGETS) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
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
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
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
        g_target_tile_mark_modes[2] = BATTLE_TILE_MARK_NONE;
        if ((g_target_tile_mark_modes[0] | g_target_tile_mark_modes[1]) == 0) {
            battle_map_modify_palette(
                MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_modify_palette(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
}

/* Render the battle terrain and weather, apply map resource records and advance mesh and texture animations. */

#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/battle_map_load.h"
#include "fft/battle_map_mesh.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect_particle.h"
#include "fft/gfx.h"
#include "fft/render.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/target.h"
#include "fft/weather.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Weather particles of g_weather_particles: snow
 * (BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES) uses all 64, rain
 * (BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES) fills two sets of 32. */
typedef struct weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} weather_particle_t;

/* Lightning flash state (g_weather_lightning_state).
 * light_matrices_set fills color_matrices[0] and [2]; the lightning weather commands
 * clear the rotation part of color_matrices[1] and start the 60-frame flash timer. */
typedef struct lightning_state {
    u32 timer;                                                       /* 0x00 */
    MATRIX color_matrices[3];                                        /* 0x04 */
    u8 _unused_64[0x20];                                             /* 0x64 */
    CVECTOR base_colors[BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT]; /* 0x84 */
    CVECTOR colors[BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT];      /* 0x8c */
} lightning_state_t;
typedef char lightning_state_size_must_be_148[(sizeof(lightning_state_t) == 148) ? 1 : -1];

/* Values of g_weather_lightning_mode. */
typedef enum lightning_mode {
    BATTLE_MAP_LIGHTNING_OFF = 0,
    BATTLE_MAP_LIGHTNING_RANDOM = 1,
    BATTLE_MAP_LIGHTNING_ONCE = 2 /* back to OFF when the flash ends */
} lightning_mode_e;

/* One instruction from a mesh animation record of type 2. A duration of
 * 0 stops the part's animation when it is reached. */
typedef struct mesh_instruction {
    u8 keyframe;  /* 1-based keyframe id */
    u8 next;      /* index of the following instruction */
    u16 duration; /* frames */
} mesh_instruction_t;

/* A mesh part's 8 animation states of 17 instructions (index 0 unused).
 * States are loaded state-major; row 0 is also the running copy that event
 * command 0x80 overwrites with the selected state. */
typedef struct mesh_instruction_list {
    mesh_instruction_t states[8][17];
} mesh_instruction_list_t;
typedef char mesh_instruction_list_size_must_be_544[(sizeof(mesh_instruction_list_t) == 544) ? 1 : -1];

/* Terrain lookup and tile-mark state. */
extern s32 g_battle_map_lowest_surface_y;
extern battle_map_tile_t* g_battle_map_tile_data_ptr;
extern u8 g_battle_map_tile_depth;
extern u8 g_battle_map_tile_width;
extern u16 g_battle_map_tile_mark_x;
extern s16 g_battle_map_tile_mark_y;
extern s16 g_battle_map_tile_mark_modes[3];

/* battle_map_tile_mark_mode_e, one per tile mark */
extern render_buffer_t* g_battle_map_tile_mark_color_buffer;

/* Weather particles and their transforms. */
extern weather_particle_t g_weather_particles[64];
extern weather_particle_t g_weather_rain_splash_particles[32];
extern u8 g_weather_primary_speed;
extern u8 g_weather_fall_speed;
extern u8 g_weather_rain_secondary_fall_speed;
extern MATRIX g_weather_matrix;
extern SVECTOR g_weather_rotation;
extern VECTOR g_weather_scale;
extern s32 g_weather_sprite_scale;
extern VECTOR g_weather_translation;
extern s32 g_weather_lightning_mode;
extern lightning_state_t g_weather_lightning_state;
extern SVECTOR g_weather_timed_transform_rotation;

/* Palette animation upload state. */
extern clut_t g_battle_map_palette_animation_frames;
extern RECT g_battle_map_vram_load_rect;

/* Working, saved and interpolated lighting matrices. */
extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern MATRIX g_battle_map_light_rotation_matrix;
extern MATRIX g_battle_map_local_light_matrix;
extern MATRIX g_battle_map_saved_light_color_matrix;            /* saved color matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;                  /* saved light matrix (command 0x5f/0x60) */
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start color matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */

/* The transition's target (command 0x66) or delta (0x68) color and light matrices. */
extern MATRIX g_battle_map_light_transition_target_color_matrix;
extern MATRIX g_battle_map_light_transition_target_light_matrix;

/* The applied background gradient pair. The copies in battle_map_light_state_command
 * move each color as an independent unaligned 32-bit value (lwl/lwr + swl/swr). */
extern battle_map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern CVECTOR g_battle_map_saved_background_gradient_first;
extern CVECTOR g_battle_map_saved_background_gradient_second;
static rgb24_t g_battle_map_ambient_polygon_color;
extern render_buffer_t* g_battle_map_ambient_restored_buffer;
extern CVECTOR g_battle_map_current_darkness_color;
extern u8 g_battle_map_back_color_disabled;
extern CVECTOR g_battle_map_cached_back_color;
extern u8 g_battle_map_polygon_flag_clear_countdown;

/* Mesh animation streams and the currently selected part. */
extern battle_map_mesh_keyframe_t g_battle_map_mesh_animation_keyframes[];
extern mesh_instruction_list_t g_battle_map_mesh_animation_instructions[9];
extern s32 g_battle_map_mesh_part_animation_states[];
extern battle_map_texture_animation_t g_battle_map_texture_animations[BATTLE_MAP_TEXTURE_ANIMATION_COUNT];
extern s32 g_battle_map_primary_textured_quad_count;
extern s32 g_battle_map_primary_textured_triangle_count;
extern s32 g_battle_map_primary_untextured_quad_count;
extern s32 g_battle_map_primary_untextured_triangle_count;
extern battle_map_mesh_triangle_positions_t* g_battle_map_part_textured_triangle_positions;
extern battle_map_mesh_quad_positions_t* g_battle_map_part_textured_quad_positions;
extern battle_map_mesh_triangle_positions_t* g_battle_map_part_untextured_triangle_positions;
extern battle_map_mesh_quad_positions_t* g_battle_map_part_untextured_quad_positions;
extern battle_map_mesh_triangle_normals_t* g_battle_map_part_textured_triangle_normals;
extern battle_map_mesh_quad_normals_t* g_battle_map_part_textured_quad_normals;
extern s32 g_battle_map_part_textured_quad_count;
extern s32 g_battle_map_part_textured_triangle_count;
extern s32 g_battle_map_part_untextured_quad_count;
extern s32 g_battle_map_part_untextured_triangle_count;

/* Map projection state. */
extern VECTOR g_battle_map_camera_zoom;
extern SVECTOR g_battle_map_camera_rotation;

/* Untextured map polygons go to OT slot (otz & mask) | fixed index: {0, 4}
 * normally, {-1, 0} while map effect data is active (command 0x88), so a
 * non-zero mask also marks that state. */
extern s32 g_battle_map_untextured_fixed_ot_index;
extern s32 g_battle_map_untextured_otz_mask;

/* Default rotation and scale copied when mesh parts or map resources are initialized. */
static const SVECTOR g_zero_svector;

static const VECTOR g_unit_scale_vector;

extern u32 g_gfx_battle_otags[2][0x180];

/* The first rain endpoint reuses fall_speed as its splash phase. */
typedef enum rain_phase {
    WEATHER_RAIN_FALLING = 0,
    WEATHER_RAIN_SPLASH_VISIBLE = 1,
    WEATHER_RAIN_SPLASH_HIDDEN = 2
} rain_phase_e;

/* Current and saved GTE background colors are three adjacent 32-bit channels. */
typedef struct back_color {
    s32 red;
    s32 green;
    s32 blue;
} back_color_t;
typedef char back_color_size_must_be_12[sizeof(back_color_t) == 12 ? 1 : -1];

extern back_color_t g_battle_map_back_color;
extern back_color_t g_battle_map_saved_back_color;

static u8 g_saved_texture_modes[72] = { 0 };

static MATRIX g_map_camera_matrix = { 0 };

static DR_MOVE g_texture_animation_moves[BATTLE_MAP_TEXTURE_ANIMATION_COUNT] = { 0 };

/* Provisional: these two GNS work arrays are cleared but have no identified reader. */
static u8 g_unread_gns_load_infos[150][14] = { 0 };

static u8 g_unread_gns_record_flags[150] = { 0 };

static void weather_particles_init(weather_command_e command);

static void mesh_animations_update(void);

static void texture_animations_update(void);

static void polygon_flags_dispatch(s32 command);

static void light_matrices_set(MATRIX* first, MATRIX* second);

static battle_map_state_record_t* animation_record_load(battle_map_state_record_t* record);

/* Apply a battle_map_polygon_flag_command_e to the textured map polygons: reset or clear
 * their per-frame lit flag, or repaint the unlit ones in the ambient color in the
 * current render buffer. */
static void polygon_flags_dispatch(s32 command) {
    s32 i;

    switch (command) {
    case BATTLE_MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                (g_render_current_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (!(g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                (g_render_current_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
            }
        }
        g_battle_map_ambient_restored_buffer = g_render_current_buffer;
        break;
    case BATTLE_MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                g_battle_map_textured_triangle_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_battle_map_textured_triangle_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                g_battle_map_textured_quad_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
            } else {
                g_battle_map_textured_quad_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
            }
        }
        break;
    case BATTLE_MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE:
        for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
            g_battle_map_textured_triangle_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        for (i = 0; i < g_battle_map_textured_quad_count; i++) {
            g_battle_map_textured_quad_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
        }
        g_battle_map_polygon_flag_clear_countdown = 0;
        break;
    }
}

/*
 * Copy two matrices into the working pair at g_battle_map_light_color_matrix and into entries 0 and
 * 2 of g_weather_lightning_state.color_matrices, then load the first into the GTE color matrix.
 *
 * Each backup copy rereads the working field; a chained assignment assigns the
 * loop pointers to different registers.
 */
static void light_matrices_set(MATRIX* first, MATRIX* second) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_color_matrix.m[i][j] = first->m[i][j];
            g_weather_lightning_state.color_matrices[0].m[i][j] = g_battle_map_light_color_matrix.m[i][j];
        }
        g_battle_map_light_color_matrix.t[i] = first->t[i];
        g_weather_lightning_state.color_matrices[0].t[i] = g_battle_map_light_color_matrix.t[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_battle_map_light_matrix.m[i][j] = second->m[i][j];
            g_weather_lightning_state.color_matrices[2].m[i][j] = g_battle_map_light_matrix.m[i][j];
        }
        g_battle_map_light_matrix.t[i] = second->t[i];
        g_weather_lightning_state.color_matrices[2].t[i] = g_battle_map_light_matrix.t[i];
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
s32* battle_map_light_control_back_color(s32 command, s32 value_1, s32 value_2, s32 value_3) {
    /* Left unset on the DISABLE, RESTORE and default paths. */
    s32* result;

    switch (command) {
    case BATTLE_MAP_LIGHT_BACK_COLOR_SET: {
        s32* stored_values;

        if (g_battle_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_battle_map_back_color.red;
        *stored_values = value_1;
        g_battle_map_back_color.green = value_2;
        g_battle_map_back_color.blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case BATTLE_MAP_LIGHT_BACK_COLOR_GET:
        result = &g_battle_map_back_color.red;
        break;
    case BATTLE_MAP_LIGHT_BACK_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_battle_map_back_color_disabled = 1;
        return result;
    case BATTLE_MAP_LIGHT_BACK_COLOR_RESTORE:
        SetBackColor(g_battle_map_back_color.red, g_battle_map_back_color.green, g_battle_map_back_color.blue);
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
    battle_map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration) {
    MATRIX* result;
    s32 i;
    s32 j;

    switch (command) {
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_SET:
        g_battle_map_light_color_matrix = *color;
        g_battle_map_light_matrix = *light;
        SetColorMatrix(&g_battle_map_light_color_matrix);
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_SAVE:
        g_battle_map_saved_light_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_saved_light_matrix = g_battle_map_light_matrix;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_RESTORE:
        g_battle_map_light_color_matrix = g_battle_map_saved_light_color_matrix;
        g_battle_map_light_matrix = g_battle_map_saved_light_matrix;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_COLOR:
        result = &g_battle_map_light_color_matrix;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT:
        result = &g_battle_map_light_matrix;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_START_BLEND:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND:
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
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_START_DELTA:
        g_battle_map_light_transition_start_color_matrix = g_battle_map_light_color_matrix;
        g_battle_map_light_transition_start_light_matrix = g_battle_map_light_matrix;
        g_battle_map_light_transition_target_color_matrix = *color;
        g_battle_map_light_transition_target_light_matrix = *light;
        g_battle_map_light_transition_frame = 0;
        g_battle_map_light_transition_duration = duration;
        g_battle_map_light_transition_command = BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA;
        break;
    case BATTLE_MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA:
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
void* battle_map_light_state_command(battle_map_light_command_e command, void* data) {
    void* result;

    switch (command) {
    case BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP]
            = ((battle_map_background_gradient_colors_t*)data)->colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP];
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM]
            = ((battle_map_background_gradient_colors_t*)data)->colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case BATTLE_MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT:
        result = &g_battle_map_background_gradient_colors;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT:
        g_battle_map_saved_background_gradient_first
            = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP];
        g_battle_map_saved_background_gradient_second
            = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM];
        break;
    case BATTLE_MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT:
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP]
            = g_battle_map_saved_background_gradient_first;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM]
            = g_battle_map_saved_background_gradient_second;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR:
        g_battle_map_back_color.red = ((CVECTOR*)data)->r;
        g_battle_map_back_color.green = ((CVECTOR*)data)->g;
        g_battle_map_back_color.blue = ((CVECTOR*)data)->b;
        SetBackColor(g_battle_map_back_color.red, g_battle_map_back_color.green, g_battle_map_back_color.blue);
        break;
    case BATTLE_MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR:
        g_battle_map_saved_back_color.red = g_battle_map_back_color.red;
        g_battle_map_saved_back_color.green = g_battle_map_back_color.green;
        g_battle_map_saved_back_color.blue = g_battle_map_back_color.blue;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR:
        g_battle_map_back_color.red = g_battle_map_saved_back_color.red;
        g_battle_map_back_color.green = g_battle_map_saved_back_color.green;
        g_battle_map_back_color.blue = g_battle_map_saved_back_color.blue;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR: {
        CVECTOR* color = &g_battle_map_cached_back_color;

        color->r = g_battle_map_back_color.red;
        g_battle_map_cached_back_color.g = g_battle_map_back_color.green;
        g_battle_map_cached_back_color.b = g_battle_map_back_color.blue;
        result = color;
        break;
    }
    case BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR:
        g_battle_map_current_darkness_color.r = ((CVECTOR*)data)->r;
        g_battle_map_current_darkness_color.g = ((CVECTOR*)data)->g;
        g_battle_map_current_darkness_color.b = ((CVECTOR*)data)->b;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR:
        g_battle_map_current_darkness_color.r = 0;
        g_battle_map_current_darkness_color.g = 0;
        g_battle_map_current_darkness_color.b = 1;
        break;
    case BATTLE_MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR:
        result = &g_battle_map_current_darkness_color;
        break;
    }
    return result;
}

#define GT3(i)       (g_render_current_buffer->gt3[i])
#define GT4(i)       (g_render_current_buffer->gt4[i])
#define SAVED_GT3(i) (g_battle_map_tile_mark_color_buffer->gt3[i])
#define SAVED_GT4(i) g_battle_map_tile_mark_color_buffer->gt4[i]

/* Queue the map mesh parts and weather particles for one frame, then apply or
 * restore the targeting and movement tile colors.
 *
 * In order: link the backdrop gradient, advance the texture and mesh part
 * animations, run a lightning flash (random strikes while lightning is on),
 * step a light matrix transition, draw part 0 with the camera alone and each
 * animated part 1-8 posed under its parent, then the weather particles (snow,
 * the E458 particle swell, or rain with splashes), repeat an ambient repaint in
 * this buffer, and finally apply the SHOW and HIDE requests of the three
 * tile-mark layers. */
void battle_map_mesh_weather_draw(MATRIX* camera) {
    MATRIX light_rotation;
    MATRIX light_matrix;
    MATRIX effect_matrix;
    RECT splash_uv[4] = { { 64, 160, 15, 15 }, { 48, 160, 15, 15 }, { 32, 160, 15, 15 }, { 16, 160, 15, 15 } };
    RECT drop_uv[3] = { { 16, 160, 15, 15 }, { 32, 160, 15, 15 }, { 48, 160, 15, 15 } };
    VECTOR effect_translation;
    VECTOR unused_vector;
    VECTOR screen;
    CVECTOR unused = { 255, 255, 255, 0 };
    s32 i;
    s32 flag;
    s32 effect_flag;
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
    battle_map_tile_t* ground_tile;

    /* The backdrop gradient and its draw modes go in ordering-table slot 0x17F, the first drawn. */
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay_modes[1]);
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay);
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay_modes[0]);

    texture_animations_update();
    mesh_animations_update();

    /* Lightning strikes at random, about once every 480 frames, with one of two thunder sounds. */
    if (g_weather_lightning_mode != BATTLE_MAP_LIGHTNING_OFF && g_weather_sfx_enabled != 0) {
        if (rand() % 480 == 0) {
            g_weather_lightning_state.timer = 60;
            if (rand() & 1) {
                g_weather_thunder_a_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_A);
                sound_free_channel_sfx_play(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_A));
            } else {
                g_weather_thunder_b_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_B);
                sound_free_channel_sfx_play(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_B));
            }
        }

        /* Over the 60-frame flash the gradient eases from the flash colors back to its base colors. */
        if (g_weather_lightning_state.timer != 0) {
            u32 remaining;

            remaining = --g_weather_lightning_state.timer;
            g_weather_lightning_state.colors[0].r = g_weather_lightning_state.base_colors[0].r
                - g_weather_lightning_state.base_colors[0].r * remaining / 60 + remaining * 208 / 60;
            g_weather_lightning_state.colors[0].g = g_weather_lightning_state.base_colors[0].g
                - g_weather_lightning_state.base_colors[0].g * remaining / 60 + remaining * 208 / 60;
            g_weather_lightning_state.colors[0].b = g_weather_lightning_state.base_colors[0].b
                - g_weather_lightning_state.base_colors[0].b * remaining / 60 + remaining * 3;
            g_weather_lightning_state.colors[1].r = g_weather_lightning_state.base_colors[1].r
                - g_weather_lightning_state.base_colors[1].r * remaining / 60 + remaining * 112 / 60;
            g_weather_lightning_state.colors[1].g = g_weather_lightning_state.base_colors[1].g
                - g_weather_lightning_state.base_colors[1].g * remaining / 60 + remaining * 112 / 60;
            g_weather_lightning_state.colors[1].b = g_weather_lightning_state.base_colors[1].b
                - g_weather_lightning_state.base_colors[1].b * remaining / 60 + remaining * 96 / 60;
            battle_map_light_state_command(
                BATTLE_MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &g_weather_lightning_state.colors[0]);
        } else {
            battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
            if (g_weather_lightning_mode == BATTLE_MAP_LIGHTNING_ONCE) {
                g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_OFF;
            }
        }

        /* Flicker the map lighting: color matrix 1 with a blue back color on frames 58 and 56,
         * the normal matrix and back color on frames 57 and 51. */
        if (g_weather_lightning_state.timer == 0x3A || g_weather_lightning_state.timer == 0x38) {
            SetColorMatrix(&g_weather_lightning_state.color_matrices[1]);
            SetBackColor(0, 0, 0x40);
        }
        if (g_weather_lightning_state.timer == 0x39 || g_weather_lightning_state.timer == 0x33) {
            SetColorMatrix(&g_weather_lightning_state.color_matrices[0]);
            SetBackColor(g_battle_map_back_color.red, g_battle_map_back_color.green, g_battle_map_back_color.blue);
        }
    }

    if (g_battle_map_light_transition_command != 0) {
        battle_map_light_matrix_command(
            g_battle_map_light_transition_command, &g_battle_map_light_color_matrix, &g_battle_map_light_matrix, 0);
    }

    /* Color the backdrop polygon: the top gradient color on its upper vertices, the bottom on the lower. */
    g_render_current_buffer->overlay.r0
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r;
    g_render_current_buffer->overlay.g0
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g;
    g_render_current_buffer->overlay.b0
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b;
    g_render_current_buffer->overlay.r1
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r;
    g_render_current_buffer->overlay.g1
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g;
    g_render_current_buffer->overlay.b1
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b;
    g_render_current_buffer->overlay.r2
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_render_current_buffer->overlay.g2
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_render_current_buffer->overlay.b2
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    g_render_current_buffer->overlay.r3
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_render_current_buffer->overlay.g3
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_render_current_buffer->overlay.b3
        = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b;

    MulMatrix0(&g_battle_map_light_matrix, &g_battle_map_light_rotation_matrix, &g_battle_map_local_light_matrix);
    SetLightMatrix(&g_battle_map_local_light_matrix);

    if (g_battle_map_polygon_flag_clear_countdown != 0) {
        if (--g_battle_map_polygon_flag_clear_countdown == 0) {
            polygon_flags_dispatch(BATTLE_MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE);
        }
    }

    /* Part 0, the static map, is drawn with the camera transform alone. */
    SetRotMatrix(camera);
    SetTransMatrix(camera);
    g_battle_map_part_textured_triangle_count = g_battle_map_mesh_parts[0].textured_triangle_count;
    g_battle_map_part_textured_triangle_positions
        = &g_battle_map_textured_triangle_positions[g_battle_map_mesh_parts[0].textured_triangle_start];
    g_battle_map_part_textured_triangle_normals
        = &g_battle_map_textured_triangle_normals[g_battle_map_mesh_parts[0].textured_triangle_start];
    battle_map_queue_textured_triangles(g_render_current_buffer->otag,
        &g_render_current_buffer->gt3[g_battle_map_mesh_parts[0].textured_triangle_start],
        g_battle_map_camera_rotation.vy, &i);

    g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts[0].textured_quad_count;
    g_battle_map_part_textured_quad_positions
        = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts[0].textured_quad_start];
    g_battle_map_part_textured_quad_normals
        = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts[0].textured_quad_start];
    battle_map_queue_textured_quads(g_render_current_buffer->otag,
        &g_render_current_buffer->gt4[g_battle_map_mesh_parts[0].textured_quad_start], g_battle_map_camera_rotation.vy,
        &i);

    g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts[0].untextured_triangle_count;
    g_battle_map_part_untextured_triangle_positions
        = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts[0].untextured_triangle_start];
    battle_map_queue_untextured_triangles(g_render_current_buffer->otag,
        &g_render_current_buffer->f3[g_battle_map_mesh_parts[0].untextured_triangle_start],
        g_battle_map_camera_rotation.vy, &i);

    g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts[0].untextured_quad_count;
    g_battle_map_part_untextured_quad_positions
        = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts[0].untextured_quad_start];
    battle_map_queue_untextured_quads(g_render_current_buffer->otag,
        &g_render_current_buffer->f4[g_battle_map_mesh_parts[0].untextured_quad_start], g_battle_map_camera_rotation.vy,
        &i);

    /* Dead store: `matrix` is reassigned for every drawn child part before
     * use. It still survives the first CSE pass (the loop uses `matrix`), and
     * the second pass reuses it for the loop-hoisted &g_battle_map_mesh_parts[0].matrix
     * constant; reload then deletes that spilled constant together with this
     * set. The transient local moves the draw calls' `&i` temporary from v0
     * to a0, as in the target. Earlier than the textured_triangle_count access, it would
     * become the base of that address instead. */
    matrix = &g_battle_map_mesh_parts[0].matrix;
    /* Parts 1-8 that have an animation are posed relative to their parent part
     * and lit by their own rotation. */
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
            battle_map_queue_textured_triangles(g_render_current_buffer->otag,
                &g_render_current_buffer->gt3[g_battle_map_mesh_parts[part].textured_triangle_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_textured_quad_count = g_battle_map_mesh_parts[part].textured_quad_count;
            g_battle_map_part_textured_quad_positions
                = &g_battle_map_textured_quad_positions[g_battle_map_mesh_parts[part].textured_quad_start];
            g_battle_map_part_textured_quad_normals
                = &g_battle_map_textured_quad_normals[g_battle_map_mesh_parts[part].textured_quad_start];
            battle_map_queue_textured_quads(g_render_current_buffer->otag,
                &g_render_current_buffer->gt4[g_battle_map_mesh_parts[part].textured_quad_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_triangle_count = g_battle_map_mesh_parts[part].untextured_triangle_count;
            g_battle_map_part_untextured_triangle_positions
                = &g_battle_map_untextured_triangle_positions[g_battle_map_mesh_parts[part].untextured_triangle_start];
            battle_map_queue_untextured_triangles(g_render_current_buffer->otag,
                &g_render_current_buffer->f3[g_battle_map_mesh_parts[part].untextured_triangle_start],
                g_battle_map_camera_rotation.vy, &i);
            g_battle_map_part_untextured_quad_count = g_battle_map_mesh_parts[part].untextured_quad_count;
            g_battle_map_part_untextured_quad_positions
                = &g_battle_map_untextured_quad_positions[g_battle_map_mesh_parts[part].untextured_quad_start];
            battle_map_queue_untextured_quads(g_render_current_buffer->otag,
                &g_render_current_buffer->f4[g_battle_map_mesh_parts[part].untextured_quad_start],
                g_battle_map_camera_rotation.vy, &i);

            /* While an ability effect runs on these three maps, keyframes 0x61 onward give four points per part;
             * transform them by the part's pose into g_effect_map_vertices. */
            if (g_battle_map_untextured_otz_mask != 0 && g_effect_map_vertices != 0
                && (g_battle_map_loaded_id == BATTLE_MAP_ID_ENTRANCE_TO_DEATH_CITY
                    || g_battle_map_loaded_id == BATTLE_MAP_ID_GRAVEYARD_OF_AIRSHIPS
                    || g_battle_map_loaded_id == BATTLE_MAP_ID_ZIREKILE_FALLS)) {
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
                        g_effect_map_vertices + j + (part - 1) * 4, &effect_flag);
                }
            }
        }
    }
    PopMatrix();

    /* Snow: 64 flakes in a slowly spinning volume; a flake that falls below the screen respawns at the top. */
    if (g_weather_effect_mode == BATTLE_MAP_WEATHER_EFFECT_SNOW) {
        PushMatrix();
        g_weather_rotation.vy += g_weather_primary_speed;
        RotMatrix(&g_weather_rotation, &g_weather_matrix);
        TransMatrix(&g_weather_matrix, &g_weather_translation);
        ScaleMatrix(&g_weather_matrix, &g_weather_scale);
        SetRotMatrix(&g_weather_matrix);
        SetTransMatrix(&g_weather_matrix);

        half_size = g_battle_map_camera_zoom.vx * g_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            /* The ground height below the flake is computed but never used in this mode. */
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (g_weather_particles[i].z / BATTLE_MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_weather_particles[i].x / BATTLE_MAP_TILE_SIZE;
            ground_tile = &g_battle_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);

            /* A flake whose last projected y (read from buffer 0) is below the screen respawns at the top
             * with a random sprite, speed, x and z. */
            if (g_render_buffers[0].ft3[i].y0 >= 0x119) {
                if (g_weather_scale.vy != 0) {
                    g_weather_particles[i].y = -0x100000 / g_weather_scale.vy;
                }
                g_weather_particles[i].fall_speed = 0;
                sprite = rand() % 3;

                g_render_buffers[0].ft3[i].u0 = drop_uv[sprite].x;
                g_render_buffers[1].ft3[i].u0 = drop_uv[sprite].x;
                g_render_buffers[0].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[1].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[0].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[1].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[0].ft3[i].v0 = drop_uv[sprite].y;
                g_render_buffers[1].ft3[i].v0 = drop_uv[sprite].y;
                g_render_buffers[0].ft3[i].v1 = drop_uv[sprite].y;
                g_render_buffers[1].ft3[i].v1 = drop_uv[sprite].y;
                g_render_buffers[0].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                g_render_buffers[1].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                g_weather_particles[i].fall_speed = sprite + g_weather_fall_speed;
                if (g_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_weather_particles[i].x = rand() % (0x8C000 / g_weather_scale.vx);
                    } else {
                        g_weather_particles[i].x = -(rand() % (0x8C000 / g_weather_scale.vx));
                    }
                }
                if (g_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_weather_particles[i].z = rand() % (0x8C000 / g_weather_scale.vz);
                    } else {
                        g_weather_particles[i].z = -(rand() % (0x8C000 / g_weather_scale.vz));
                    }
                }
            }

            /* Drift one unit in x or z, each about once every 32 frames. */
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_weather_particles[i].x = g_weather_particles[i].x + 1;
                } else {
                    g_weather_particles[i].x = g_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_weather_particles[i].z = g_weather_particles[i].z + 1;
                } else {
                    g_weather_particles[i].z = g_weather_particles[i].z - 1;
                }
            }

            g_weather_particles[i].y += g_weather_particles[i].fall_speed * g_system_vsync_interval;
            RotTrans((SVECTOR*)&g_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_render_current_buffer->ft3[i].x1 = sx + half_size;
                g_render_current_buffer->ft3[i].x2 = sx + half_size;
                g_render_current_buffer->ft3[i].y2 = sy + half_size;
                g_render_current_buffer->ft3[i].x0 = sx;
                g_render_current_buffer->ft3[i].y0 = sy;
                g_render_current_buffer->ft3[i].y1 = sy;
                AddPrim(g_render_current_buffer->otag + 2, &g_render_current_buffer->ft3[i]);
            }
        }
        PopMatrix();
    }

    /* Map command 0x96 (E458): the volume swells and rolls over the duration;
     * particles fall during its first eighth and rise after. */
    if (g_weather_effect_mode == BATTLE_MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
        s32 sine;

        PushMatrix();
        g_weather_timed_transform_rotation.vy = g_battle_map_camera_rotation.vy;
        g_weather_timed_transform_rotation.vz
            = -(rsin((g_weather_timed_transform_frame << 10) / g_weather_timed_transform_duration) >> 4);
        sine = rsin((g_weather_timed_transform_frame << 10) / g_weather_timed_transform_duration);
        g_weather_scale.vx = sine * 4 + ONE;
        g_weather_scale.vz = sine * 4 + ONE;
        g_weather_scale.vy = sine + ONE;
        g_weather_timed_transform_frame++;
        RotMatrix(&g_weather_timed_transform_rotation, &g_weather_matrix);
        TransMatrix(&g_weather_matrix, &g_weather_translation);
        ScaleMatrix(&g_weather_matrix, &g_weather_scale);
        SetRotMatrix(&g_weather_matrix);
        SetTransMatrix(&g_weather_matrix);

        half_size = g_battle_map_camera_zoom.vx * g_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            /* Both ground heights are computed and discarded, as in snow. */
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (g_weather_particles[i].z / BATTLE_MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_weather_particles[i].x / BATTLE_MAP_TILE_SIZE;
            ground_tile = &g_battle_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
            tile = (g_weather_particles[i + 32].z / BATTLE_MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_weather_particles[i + 32].x / BATTLE_MAP_TILE_SIZE;
            ground_tile = &g_battle_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);

            /* Respawn particles that leave the screen: at the top while falling, at the bottom while rising. */
            if ((g_weather_timed_transform_frame < g_weather_timed_transform_duration / 8
                    && g_render_buffers[0].ft3[i].y0 >= 0x119)
                || (g_weather_timed_transform_duration / 8 < g_weather_timed_transform_frame
                    && g_render_buffers[0].ft3[i].y0 < 0)) {
                if (g_weather_scale.vy != 0) {
                    if (g_weather_timed_transform_frame < g_weather_timed_transform_duration / 8) {
                        g_weather_particles[i].y = -0x100000 / g_weather_scale.vy;
                    }
                    if (g_weather_timed_transform_duration / 8 < g_weather_timed_transform_frame) {
                        g_weather_particles[i].y = 0x30000 / g_weather_scale.vy;
                    }
                }
                sprite = rand() % 3;
                g_weather_particles[i].fall_speed = g_weather_fall_speed + 3 - sprite;

                g_render_buffers[0].ft3[i].u0 = drop_uv[sprite].x;
                g_render_buffers[1].ft3[i].u0 = drop_uv[sprite].x;
                g_render_buffers[0].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[1].ft3[i].u1 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[0].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[1].ft3[i].u2 = drop_uv[sprite].x + drop_uv[sprite].w;
                g_render_buffers[0].ft3[i].v0 = drop_uv[sprite].y;
                g_render_buffers[1].ft3[i].v0 = drop_uv[sprite].y;
                g_render_buffers[0].ft3[i].v1 = drop_uv[sprite].y;
                g_render_buffers[1].ft3[i].v1 = drop_uv[sprite].y;
                g_render_buffers[0].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                g_render_buffers[1].ft3[i].v2 = drop_uv[sprite].y + drop_uv[sprite].h;
                if (g_weather_scale.vx != 0) {
                    if (rand() & 1) {
                        g_weather_particles[i].x = rand() % (0x8C000 / g_weather_scale.vx);
                    } else {
                        g_weather_particles[i].x = -(rand() % (0x8C000 / g_weather_scale.vx));
                    }
                }
                if (g_weather_scale.vz != 0) {
                    if (rand() & 1) {
                        g_weather_particles[i].z = rand() % (0x8C000 / g_weather_scale.vz);
                    } else {
                        g_weather_particles[i].z = -(rand() % (0x8C000 / g_weather_scale.vz));
                    }
                }
            }

            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_weather_particles[i].x = g_weather_particles[i].x + 1;
                } else {
                    g_weather_particles[i].x = g_weather_particles[i].x - 1;
                }
            }
            if ((rand() & 0x1F) == 0) {
                if (rand() & 1) {
                    g_weather_particles[i].z = g_weather_particles[i].z + 1;
                } else {
                    g_weather_particles[i].z = g_weather_particles[i].z - 1;
                }
            }

            if (g_weather_timed_transform_frame < g_weather_timed_transform_duration / 8) {
                g_weather_particles[i].y += g_weather_particles[i].fall_speed * g_system_vsync_interval
                    * rsin((g_weather_timed_transform_frame << 13) / g_weather_timed_transform_duration + 0x400) / 2048;
            }
            if (g_weather_timed_transform_duration / 8 < g_weather_timed_transform_frame) {
                g_weather_particles[i].y -= g_weather_particles[i].fall_speed * g_system_vsync_interval;
            }

            RotTrans((SVECTOR*)&g_weather_particles[i], &screen, &flag);
            depth = screen.vz / 4;
            if (depth > 0 && depth < 0x180) {
                sx = screen.vx;
                sy = screen.vy;
                g_render_current_buffer->ft3[i].x1 = sx + half_size;
                g_render_current_buffer->ft3[i].x2 = sx + half_size;
                g_render_current_buffer->ft3[i].y2 = sy + half_size;
                g_render_current_buffer->ft3[i].x0 = sx;
                g_render_current_buffer->ft3[i].y0 = sy;
                g_render_current_buffer->ft3[i].y1 = sy;
                AddPrim(g_render_current_buffer->otag + 2, &g_render_current_buffer->ft3[i]);
            }
        }

        /* At the end, restore the script weather and the default volume. That can switch the mode to
         * rain mid-frame; the rain block below then relinks the ft3 drops already queued (QUIRKS.md). */
        if (g_weather_timed_transform_frame == g_weather_timed_transform_duration) {
            weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
            g_weather_scale.vx = ONE;
            g_weather_scale.vy = ONE;
            g_weather_scale.vz = ONE;
            g_weather_rotation.vx = 0x10;
            g_weather_rotation.vz = 0x10;
            g_weather_translation.vx = 0x100;
            g_weather_translation.vy = 0xF0;
            g_weather_rotation.vy = 0;
            g_weather_translation.vz = 0x280;
        }
        PopMatrix();
    }

    /* Rain: streak i runs from particle i (lower end) up to particle i + 32. Its fall_speed
     * holds rain_phase_e; the splash record uses that field as a frame countdown. */
    if (g_weather_effect_mode == BATTLE_MAP_WEATHER_EFFECT_RAIN) {
        half_size = g_battle_map_camera_zoom.vx * 22;
        half_size /= 0x2000;
        for (i = 0; i < 32; i++) {
            g_battle_map_tile_data_ptr = g_battle_map_tile_data;
            tile = (g_weather_particles[i].z / BATTLE_MAP_TILE_SIZE) * g_battle_map_tile_width
                + g_weather_particles[i].x / BATTLE_MAP_TILE_SIZE;
            ground_tile = &g_battle_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);

            /* A streak whose upper end passed the ground respawns at a random spot high above the map. */
            if (ground < g_weather_particles[i + 32].y) {
                g_weather_particles[i].fall_speed = WEATHER_RAIN_FALLING;
                g_weather_particles[i].x = rand() % (g_battle_map_tile_width * BATTLE_MAP_TILE_SIZE);
                g_weather_particles[i].z = rand() % (g_battle_map_tile_depth * BATTLE_MAP_TILE_SIZE);
                spawn_y = -0x180;
                g_weather_particles[i].y = spawn_y;
                g_weather_particles[i + 32].x = g_weather_particles[i].x;
                g_weather_particles[i + 32].z = g_weather_particles[i].z;
                g_weather_particles[i + 32].y
                    = spawn_y - g_weather_fall_speed - rand() % (g_weather_primary_speed * 12);
            }

            g_weather_particles[i].y += g_weather_fall_speed * g_system_vsync_interval;
            g_weather_particles[i + 32].y += g_weather_fall_speed * g_system_vsync_interval;
            if (i < 16) {
                g_weather_particles[i].y += g_weather_primary_speed * g_system_vsync_interval;
                g_weather_particles[i + 32].y += g_weather_primary_speed * g_system_vsync_interval;
            } else {
                g_weather_particles[i].y += g_weather_rain_secondary_fall_speed * g_system_vsync_interval;
                g_weather_particles[i + 32].y += g_weather_rain_secondary_fall_speed * g_system_vsync_interval;
            }

            /* A lower end past the ground is parked at g_battle_map_lowest_surface_y, which the splash test
             * below recognizes; a streak still above the ground is drawn as a thin triangle. */
            if (g_weather_particles[i].y >= -0x17F) {
                if (ground < g_weather_particles[i].y) {
                    g_weather_particles[i].y = g_battle_map_lowest_surface_y;
                } else if (g_weather_particles[i + 32].y < ground) {
                    RotTrans((SVECTOR*)&g_weather_particles[i], &screen, &flag);
                    sx = screen.vx;
                    sy = screen.vy;
                    g_render_current_buffer->ft3[i].x2 = sx + 1;
                    g_render_current_buffer->ft3[i].x0 = sx;
                    g_render_current_buffer->ft3[i].y0 = sy;
                    g_render_current_buffer->ft3[i].y2 = sy - 1;
                    RotTrans((SVECTOR*)&g_weather_particles[i + 32], &screen, &flag);
                    depth = screen.vz / 4;
                    if (depth > 0 && depth < 0x180) {
                        sx = screen.vx;
                        sy = screen.vy;
                        g_render_current_buffer->ft3[i].x1 = sx;
                        g_render_current_buffer->ft3[i].y1 = sy;
                        AddPrim(g_render_current_buffer->otag + depth, &g_render_current_buffer->ft3[i]);
                    }
                }
            }

            /* A falling streak that reaches the ground starts a 16-frame splash, hidden on untargetable tiles. */
            if (g_weather_particles[i].fall_speed == WEATHER_RAIN_FALLING
                && ((ground < g_weather_particles[i].y && g_weather_particles[i + 32].y < ground)
                    || g_weather_particles[i].y == g_battle_map_lowest_surface_y)) {
                g_weather_rain_splash_particles[i].fall_speed = 0x10;
                g_weather_rain_splash_particles[i].y = ground - 8;
                g_weather_rain_splash_particles[i].x = g_weather_particles[i].x;
                g_weather_rain_splash_particles[i].z = g_weather_particles[i].z;
                if (!(g_battle_map_tile_data_ptr[tile].flags.untargetable)) {
                    g_weather_particles[i].fall_speed = WEATHER_RAIN_SPLASH_VISIBLE;
                } else {
                    g_weather_particles[i].fall_speed = WEATHER_RAIN_SPLASH_HIDDEN;
                }
            }

            /* The splash uses the streak's second primitive (i + 32) and changes sprite every fourth frame. */
            if (g_weather_rain_splash_particles[i].fall_speed != 0) {
                RotTrans((SVECTOR*)&g_weather_rain_splash_particles[i], &screen, &flag);
                depth = screen.vz / 4;
                if (depth > 0 && depth < 0x180) {
                    sx = screen.vx;
                    sy = screen.vy;
                    splash_y = sy - 4;
                    g_render_current_buffer->ft3[i + 32].x0 = sx - half_size;
                    g_render_current_buffer->ft3[i + 32].y0 = splash_y - half_size;
                    g_render_current_buffer->ft3[i + 32].x1 = sx + half_size;
                    g_render_current_buffer->ft3[i + 32].y1 = splash_y + half_size;
                    g_render_current_buffer->ft3[i + 32].x2 = sx - half_size;
                    g_render_current_buffer->ft3[i + 32].y2 = splash_y + half_size;
                    if (g_weather_particles[i].fall_speed == WEATHER_RAIN_SPLASH_VISIBLE) {
                        AddPrim(g_render_current_buffer->otag + depth, &g_render_current_buffer->ft3[i + 32]);
                    }
                }

                g_weather_rain_splash_particles[i].fall_speed--;
                frame = g_weather_rain_splash_particles[i].fall_speed;
                if ((frame & 3) == 0) {
                    g_render_buffers[0].ft3[i + 32].u0 = splash_uv[(frame >> 2) & 3].x;
                    g_render_buffers[1].ft3[i + 32].u0 = splash_uv[(frame >> 2) & 3].x;
                    g_render_buffers[0].ft3[i + 32].u1 = splash_uv[(frame >> 2) & 3].x + splash_uv[(frame >> 2) & 3].w;
                    g_render_buffers[1].ft3[i + 32].u1 = splash_uv[(frame >> 2) & 3].x + splash_uv[(frame >> 2) & 3].w;
                    g_render_buffers[0].ft3[i + 32].u2 = splash_uv[(frame >> 2) & 3].x;
                    g_render_buffers[1].ft3[i + 32].u2 = splash_uv[(frame >> 2) & 3].x;
                    g_render_buffers[0].ft3[i + 32].v0 = splash_uv[(frame >> 2) & 3].y;
                    g_render_buffers[1].ft3[i + 32].v0 = splash_uv[(frame >> 2) & 3].y;
                    g_render_buffers[0].ft3[i + 32].v1 = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_render_buffers[1].ft3[i + 32].v1 = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_render_buffers[0].ft3[i + 32].v2 = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                    g_render_buffers[1].ft3[i + 32].v2 = splash_uv[(frame >> 2) & 3].y + splash_uv[(frame >> 2) & 3].h;
                }

                if (g_weather_rain_splash_particles[i].fall_speed == 0) {
                    g_weather_particles[i].fall_speed = WEATHER_RAIN_FALLING;
                }
            }
        }
    }

    /* The ambient color restores use pointer arithmetic on the decayed arrays
     * rather than GT3(i)/GT4(i) indexing: that keeps the target's
     * `addu offset, base` operand order (as in target_set_tile_background_color). */
    if (g_battle_map_ambient_restored_buffer != 0) {
        /* Ambient colors repainted in one render buffer are repeated once in the other. */
        if (g_battle_map_ambient_restored_buffer != g_render_current_buffer) {
            for (i = 0; i < g_battle_map_textured_triangle_count; i++) {
                if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                    (g_render_current_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                }
            }
            for (i = 0; i < g_battle_map_textured_quad_count; i++) {
                if (!(g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                    (g_render_current_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                }
            }
        }
        g_battle_map_ambient_restored_buffer = 0;
    }

    /* target_tile_mark_colors_set recolors one render buffer; a SHOW it leaves is
     * repeated here once on this frame's buffer, the other of the pair, then marked shown. */
    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    if (g_battle_map_tile_mark_modes[0] == BATTLE_MAP_TILE_MARK_SHOW_MOVE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                /* terrain_tile holds z in bits 1-7, x in bits 8-15 and the upper level in bit 0 (tiles 0x100 on). */
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    /* Keep the CLUT's x and switch it to the marked-tiles palette row. */
                    GT3(i).clut = (GT3(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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

        g_battle_map_tile_mark_modes[0] = BATTLE_MAP_TILE_MARK_MOVE_SHOWN;
    }

    if (g_battle_map_tile_mark_modes[1] == BATTLE_MAP_TILE_MARK_SHOW_RANGE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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

        g_battle_map_tile_mark_modes[1] = BATTLE_MAP_TILE_MARK_RANGE_SHOWN;
    }

    if (g_battle_map_tile_mark_modes[2] == BATTLE_MAP_TILE_MARK_SHOW_TARGETS) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    GT3(i).clut = (GT3(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    GT4(i).clut = (GT4(i).clut & 0x803F) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
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

        g_battle_map_tile_mark_modes[2] = BATTLE_MAP_TILE_MARK_TARGETS_SHOWN;
    }

    /* A HIDE copies the CLUTs, and some colors, back from the buffer
     * target_tile_mark_colors_set restored. */
    if (g_battle_map_tile_mark_modes[0] == BATTLE_MAP_TILE_MARK_HIDE_MOVE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
            if (!(g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                GT3(i).r0 = SAVED_GT3(i).r0;
                GT3(i).r1 = SAVED_GT3(i).r1;
                GT3(i).r2 = SAVED_GT3(i).r2;
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
            if (!(g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                GT4(i).r0 = SAVED_GT4(i).r0;
                GT4(i).r1 = SAVED_GT4(i).r1;
                GT4(i).r2 = SAVED_GT4(i).r2;
                GT4(i).r3 = SAVED_GT4(i).r3;
            }
        }

        g_battle_map_tile_mark_modes[0] = BATTLE_MAP_TILE_MARK_NONE;
        /* With no layer left, stop the marked-tiles palette pulse and restore its colors. */
        if ((g_battle_map_tile_mark_modes[1] | g_battle_map_tile_mark_modes[2]) == 0) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_STOP_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_RESTORE, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }

    if (g_battle_map_tile_mark_modes[1] == BATTLE_MAP_TILE_MARK_HIDE_RANGE) {
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            GT3(i).clut = SAVED_GT3(i).clut;
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            GT4(i).clut = SAVED_GT4(i).clut;
        }

        g_battle_map_tile_mark_modes[1] = BATTLE_MAP_TILE_MARK_NONE;
        if ((g_battle_map_tile_mark_modes[0] | g_battle_map_tile_mark_modes[2]) == 0) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_STOP_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_RESTORE, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }

    if (g_battle_map_tile_mark_modes[2] == BATTLE_MAP_TILE_MARK_HIDE_TARGETS) {
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

        g_battle_map_tile_mark_modes[2] = BATTLE_MAP_TILE_MARK_NONE;
        if ((g_battle_map_tile_mark_modes[0] | g_battle_map_tile_mark_modes[1]) == 0) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_STOP_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_RESTORE, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
}

/* Recolor the current render buffer's map polygons for one tile-mark layer
 * (battle_map_tile_mark_mode_e): SHOW tints marked tiles blue (move), red (range) or yellow (targets),
 * HIDE restores them (a hidden target tile in range turns red again) and NONE restores every
 * polygon. The first layer shown starts the marked-tiles palette pulse; the second argument is
 * unused. */
void target_tile_mark_colors_set(s32 mode, s32 unused) {
    s32 i;
    s16 other_state_a;
    s16 other_state_b;
    s16 tile;
    battle_map_tile_marks_t marks;

    g_battle_map_tile_data_ptr = g_battle_map_tile_data;
    switch (mode) {
    case BATTLE_MAP_TILE_MARK_NONE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            g_render_current_buffer->gt3[i].clut
                = (g_render_current_buffer->gt3[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
        }
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            g_render_current_buffer->gt4[i].clut
                = (g_render_current_buffer->gt4[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
        }

        /* Leave every layer in HIDE so the next draw copies this buffer's colors into the other one. */
        g_battle_map_tile_mark_modes[0] = BATTLE_MAP_TILE_MARK_HIDE_MOVE;
        g_battle_map_tile_mark_modes[1] = BATTLE_MAP_TILE_MARK_HIDE_RANGE;
        g_battle_map_tile_mark_modes[2] = BATTLE_MAP_TILE_MARK_HIDE_TARGETS;
        polygon_flags_dispatch(BATTLE_MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        polygon_flags_dispatch(BATTLE_MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT);
        g_battle_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case BATTLE_MAP_TILE_MARK_SHOW_MOVE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt3[i].clut = (g_render_current_buffer->gt3[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x20;
                    g_render_current_buffer->gt3[i].r1 = 0x20;
                    g_render_current_buffer->gt3[i].r2 = 0x20;
                    g_render_current_buffer->gt3[i].g0 = 0x28;
                    g_render_current_buffer->gt3[i].g1 = 0x28;
                    g_render_current_buffer->gt3[i].g2 = 0x28;
                    g_render_current_buffer->gt3[i].b0 = 0x60;
                    g_render_current_buffer->gt3[i].b1 = 0x60;
                    g_render_current_buffer->gt3[i].b2 = 0x60;
                    /* Clear LIT_ACTIVE so the queue routines do not relight over the tint. */
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt4[i].clut = (g_render_current_buffer->gt4[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt4[i].r0 = 0x20;
                    g_render_current_buffer->gt4[i].r1 = 0x20;
                    g_render_current_buffer->gt4[i].r2 = 0x20;
                    g_render_current_buffer->gt4[i].r3 = 0x20;
                    g_render_current_buffer->gt4[i].g0 = 0x28;
                    g_render_current_buffer->gt4[i].g1 = 0x28;
                    g_render_current_buffer->gt4[i].g2 = 0x28;
                    g_render_current_buffer->gt4[i].g3 = 0x28;
                    g_render_current_buffer->gt4[i].b0 = 0x60;
                    g_render_current_buffer->gt4[i].b1 = 0x60;
                    g_render_current_buffer->gt4[i].b2 = 0x60;
                    g_render_current_buffer->gt4[i].b3 = 0x60;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_battle_map_tile_mark_modes[0] = BATTLE_MAP_TILE_MARK_SHOW_MOVE;
        /* Start the marked-tiles palette pulse unless another layer is already shown. */
        other_state_a = g_battle_map_tile_mark_modes[1];
        other_state_b = g_battle_map_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_MAP_TILE_MARK_HIDE_RANGE
                && other_state_b == BATTLE_MAP_TILE_MARK_HIDE_TARGETS)) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_MAP_TILE_MARK_HIDE_MOVE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                    /* A lit polygon is relit from its normals again; an unlit one returns to the ambient color. */
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        g_battle_map_tile_mark_modes[0] = BATTLE_MAP_TILE_MARK_HIDE_MOVE;
        g_battle_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case BATTLE_MAP_TILE_MARK_SHOW_RANGE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt3[i].clut = (g_render_current_buffer->gt3[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x60;
                    g_render_current_buffer->gt3[i].r1 = 0x60;
                    g_render_current_buffer->gt3[i].r2 = 0x60;
                    g_render_current_buffer->gt3[i].g0 = 0x18;
                    g_render_current_buffer->gt3[i].g1 = 0x18;
                    g_render_current_buffer->gt3[i].g2 = 0x18;
                    g_render_current_buffer->gt3[i].b0 = 0x20;
                    g_render_current_buffer->gt3[i].b1 = 0x20;
                    g_render_current_buffer->gt3[i].b2 = 0x20;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt4[i].clut = (g_render_current_buffer->gt4[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt4[i].r0 = 0x60;
                    g_render_current_buffer->gt4[i].r1 = 0x60;
                    g_render_current_buffer->gt4[i].r2 = 0x60;
                    g_render_current_buffer->gt4[i].r3 = 0x60;
                    g_render_current_buffer->gt4[i].g0 = 0x18;
                    g_render_current_buffer->gt4[i].g1 = 0x18;
                    g_render_current_buffer->gt4[i].g2 = 0x18;
                    g_render_current_buffer->gt4[i].g3 = 0x18;
                    g_render_current_buffer->gt4[i].b0 = 0x20;
                    g_render_current_buffer->gt4[i].b1 = 0x20;
                    g_render_current_buffer->gt4[i].b2 = 0x20;
                    g_render_current_buffer->gt4[i].b3 = 0x20;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_battle_map_tile_mark_modes[1] = BATTLE_MAP_TILE_MARK_SHOW_RANGE;
        other_state_a = g_battle_map_tile_mark_modes[0];
        other_state_b = g_battle_map_tile_mark_modes[2];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_MAP_TILE_MARK_HIDE_MOVE
                && other_state_b == BATTLE_MAP_TILE_MARK_HIDE_TARGETS)) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_MAP_TILE_MARK_HIDE_RANGE:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_triangle_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                    if (g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                        g_battle_map_textured_quad_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        g_battle_map_tile_mark_modes[1] = BATTLE_MAP_TILE_MARK_HIDE_RANGE;
        g_battle_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case BATTLE_MAP_TILE_MARK_SHOW_TARGETS:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt3[i].clut = (g_render_current_buffer->gt3[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x50;
                    g_render_current_buffer->gt3[i].r1 = 0x50;
                    g_render_current_buffer->gt3[i].r2 = 0x50;
                    g_render_current_buffer->gt3[i].g0 = 0x60;
                    g_render_current_buffer->gt3[i].g1 = 0x60;
                    g_render_current_buffer->gt3[i].g2 = 0x60;
                    g_render_current_buffer->gt3[i].b0 = 0x10;
                    g_render_current_buffer->gt3[i].b1 = 0x10;
                    g_render_current_buffer->gt3[i].b2 = 0x10;
                    g_battle_map_textured_triangle_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt4[i].clut = (g_render_current_buffer->gt4[i].clut & 0x803f)
                        | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt4[i].r0 = 0x50;
                    g_render_current_buffer->gt4[i].r1 = 0x50;
                    g_render_current_buffer->gt4[i].r2 = 0x50;
                    g_render_current_buffer->gt4[i].r3 = 0x50;
                    g_render_current_buffer->gt4[i].g0 = 0x60;
                    g_render_current_buffer->gt4[i].g1 = 0x60;
                    g_render_current_buffer->gt4[i].g2 = 0x60;
                    g_render_current_buffer->gt4[i].g3 = 0x60;
                    g_render_current_buffer->gt4[i].b0 = 0x10;
                    g_render_current_buffer->gt4[i].b1 = 0x10;
                    g_render_current_buffer->gt4[i].b2 = 0x10;
                    g_render_current_buffer->gt4[i].b3 = 0x10;
                    g_battle_map_textured_quad_positions[i].polygon_flags &= ~BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_battle_map_tile_mark_modes[2] = BATTLE_MAP_TILE_MARK_SHOW_TARGETS;
        other_state_a = g_battle_map_tile_mark_modes[0];
        other_state_b = g_battle_map_tile_mark_modes[1];
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == BATTLE_MAP_TILE_MARK_HIDE_MOVE && other_state_b == BATTLE_MAP_TILE_MARK_HIDE_RANGE)) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 8, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case BATTLE_MAP_TILE_MARK_HIDE_TARGETS:
        for (i = 0; i < g_battle_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_battle_map_textured_triangle_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    /* A target tile still in ability range goes back to the range tint instead. */
                    if (!(marks.ability_range)) {
                        g_render_current_buffer->gt3[i].clut = (g_render_current_buffer->gt3[i].clut & 0x803f)
                            | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                        if (g_battle_map_textured_triangle_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                            g_battle_map_textured_triangle_positions[i].polygon_flags
                                |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_render_current_buffer->gt3 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt3 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt3 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                        }
                    } else {
                        g_render_current_buffer->gt3[i].r0 = 0x60;
                        g_render_current_buffer->gt3[i].r1 = 0x60;
                        g_render_current_buffer->gt3[i].r2 = 0x60;
                        g_render_current_buffer->gt3[i].g0 = 0x18;
                        g_render_current_buffer->gt3[i].g1 = 0x18;
                        g_render_current_buffer->gt3[i].g2 = 0x18;
                        g_render_current_buffer->gt3[i].b0 = 0x20;
                        g_render_current_buffer->gt3[i].b1 = 0x20;
                        g_render_current_buffer->gt3[i].b2 = 0x20;
                    }
                }
            }
        }

        for (i = 0; i < g_battle_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_battle_map_textured_quad_positions[i].terrain_tile != BATTLE_MAP_POLYGON_TILE_NONE) {
                g_battle_map_tile_mark_y = (g_battle_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_battle_map_tile_mark_x = g_battle_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_battle_map_tile_mark_y * g_battle_map_tile_width + g_battle_map_tile_mark_x;
                if (g_battle_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_battle_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_render_current_buffer->gt4[i].clut = (g_render_current_buffer->gt4[i].clut & 0x803f)
                            | BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_MAP, 0);
                        if (g_battle_map_textured_quad_positions[i].polygon_flags & BATTLE_MAP_POLYGON_FLAG_LIT) {
                            g_battle_map_textured_quad_positions[i].polygon_flags |= BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_render_current_buffer->gt4 + i)->r0 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g0 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b0 = g_battle_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r1 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g1 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b1 = g_battle_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r2 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g2 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b2 = g_battle_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r3 = g_battle_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g3 = g_battle_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b3 = g_battle_map_ambient_polygon_color.b;
                        }
                    } else {
                        g_render_current_buffer->gt4[i].r0 = 0x60;
                        g_render_current_buffer->gt4[i].r1 = 0x60;
                        g_render_current_buffer->gt4[i].r2 = 0x60;
                        g_render_current_buffer->gt4[i].r3 = 0x60;
                        g_render_current_buffer->gt4[i].g0 = 0x18;
                        g_render_current_buffer->gt4[i].g1 = 0x18;
                        g_render_current_buffer->gt4[i].g2 = 0x18;
                        g_render_current_buffer->gt4[i].g3 = 0x18;
                        g_render_current_buffer->gt4[i].b0 = 0x20;
                        g_render_current_buffer->gt4[i].b1 = 0x20;
                        g_render_current_buffer->gt4[i].b2 = 0x20;
                        g_render_current_buffer->gt4[i].b3 = 0x20;
                    }
                }
            }
        }

        g_battle_map_tile_mark_modes[2] = BATTLE_MAP_TILE_MARK_HIDE_TARGETS;
        g_battle_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    }
}

static const SVECTOR g_zero_svector = { 0, 0, 0, 0 };

static const VECTOR g_unit_scale_vector = { ONE, ONE, ONE, 0 };

/* Initializes both battle render buffers: every primitive in each pool gets its
 * GPU command code, the full-screen Gouraud overlay gets its corners and two
 * semi-transparent draw modes, and the ordering table pointers are bound.
 *
 * The function-scope counters are intentionally reused by each pool loop:
 * their lifetime reproduces the target register allocation. */
void render_buffers_init(void) {
    POLY_F3 prototype;
    s32 primitive_index;
    s32 buffer_index;

    g_render_current_buffer = &g_render_buffers[0];
    g_render_buffers[0].otag = g_gfx_battle_otags[0];
    g_render_buffers[1].otag = g_gfx_battle_otags[1];
    {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            for (primitive_index = 0; primitive_index < RENDER_WEATHER_SPRITE_COUNT; primitive_index++) {
                SetPolyFT3(&g_render_buffers[buffer_index].ft3[primitive_index]);
            }
        }
    }
    SetPolyF3(&prototype);
    g_battle_map_current_darkness_color.r = 0;
    g_battle_map_current_darkness_color.g = 0;
    g_battle_map_current_darkness_color.b = 1;
    g_battle_map_current_darkness_color.cd = prototype.code;
    SetPolyG4(&g_render_buffers[0].overlay);
    SetPolyG4(&g_render_buffers[1].overlay);
    setXY4(&g_render_buffers[0].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    setXY4(&g_render_buffers[1].overlay, 0, 0, 0x180, 0, 0, 0xF0, 0x180, 0xF0);
    SetDrawMode(&g_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    {
        for (primitive_index = 0; primitive_index < RENDER_TEXTURED_TRIANGLE_COUNT; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT3(&g_render_buffers[buffer_index].gt3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < RENDER_TEXTURED_QUAD_COUNT; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyGT4(&g_render_buffers[buffer_index].gt4[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < RENDER_UNTEXTURED_TRIANGLE_COUNT; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF3(&g_render_buffers[buffer_index].f3[primitive_index]);
            }
        }
    }
    {
        for (primitive_index = 0; primitive_index < RENDER_UNTEXTURED_QUAD_COUNT; primitive_index++) {
            for (buffer_index = 0; buffer_index < 2; buffer_index++) {
                SetPolyF4(&g_render_buffers[buffer_index].f4[primitive_index]);
            }
        }
    }
    g_weather_rain_sfx_id = 0;
    g_weather_sfx_enabled = 0;
    g_effect_map_vertices = 0;
}

/* Reset the map render state before a new GNS map loads: no polygons or tile links, every textured
 * polygon lit, ambient color 0x80 gray, weather and lightning off, texture and mesh part
 * animations stopped, GNS resource records cleared. */
void battle_map_mesh_reset_render_state(void) {
    SVECTOR zero_vector = g_zero_svector;
    VECTOR unit_scale = g_unit_scale_vector;
    s32 i;
    s16 no_tile;
    s32 j;
    s32* weather_flags;

    g_battle_map_tile_width = 1;
    g_battle_map_tile_depth = 1;
    g_battle_map_tile_mark_x = 1;
    g_battle_map_tile_mark_y = 1;
    g_battle_map_untextured_otz_mask = 0;
    g_battle_map_untextured_fixed_ot_index = 4;
    g_battle_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            g_unread_gns_load_infos[i][j] = 0;
        }
        g_unread_gns_record_flags[i] = 0;
    }
    for (i = 0; i < GNS_RESOURCE_TABLE_COUNT; i++) {
        bzero(g_battle_map_loaded_gns_records[i].load_info.bytes, sizeof(battle_map_gns_load_info_t));
        bzero(&g_battle_map_pending_gns_records[i].resource_type, 14);
    }
    weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_CLEAR);
    g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_OFF;
    /* Clear BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER and the palette modifier above the low flags. */
    weather_flags = &g_weather_flags;
    *weather_flags &= BATTLE_MAP_WEATHER_LOW_FLAGS_MASK & ~BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER;
    SetDrawMode(&g_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    g_battle_map_ambient_polygon_color.r = 0x80;
    g_battle_map_ambient_polygon_color.g = 0x80;
    g_battle_map_ambient_polygon_color.b = 0x80;
    for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
        g_battle_map_textured_triangle_positions[i].polygon_flags
            = BATTLE_MAP_POLYGON_FLAG_LIT | BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
    }
    for (i = 0; i < RENDER_TEXTURED_QUAD_COUNT; i++) {
        g_battle_map_textured_quad_positions[i].polygon_flags
            = BATTLE_MAP_POLYGON_FLAG_LIT | BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE;
    }
    for (i = 0; i < RENDER_UNTEXTURED_TRIANGLE_COUNT; i++) {
        g_battle_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < RENDER_UNTEXTURED_QUAD_COUNT; i++) {
        g_battle_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < BATTLE_MAP_TEXTURE_ANIMATION_COUNT; i++) {
        g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE;
        g_battle_map_texture_animations[i].active = 0;
    }
    /* A signed sentinel local retains each array's original -2 constant load. */
    no_tile = BATTLE_MAP_POLYGON_TILE_NONE;
    for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
        g_battle_map_textured_triangle_positions[i].terrain_tile = no_tile;
    }
    no_tile = BATTLE_MAP_POLYGON_TILE_NONE;
    for (i = 0; i < RENDER_TEXTURED_QUAD_COUNT; i++) {
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
 * g_camera_integer_coords (the negated camera position) at the screen
 * offset. It is stored in *camera_matrix, g_map_camera_matrix and map mesh
 * part 0; the angles and zoom are kept in g_battle_map_camera_rotation and
 * g_battle_map_camera_zoom, and the light rotation matrix is rebuilt. The GTE is
 * left with the scaled rotation and the bare screen offset as its translation. */
void camera_matrices_update(MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom) {
    s32 result;

    g_battle_map_camera_zoom = *camera_zoom;
    memcpy(&g_battle_map_camera_rotation, rotation, sizeof(g_battle_map_camera_rotation));

    RotMatrix(rotation, camera_matrix);
    TransMatrix(camera_matrix, screen_offset);
    ScaleMatrix(camera_matrix, &g_battle_map_camera_zoom);
    RotMatrix(rotation, &g_map_camera_matrix);
    SetRotMatrix(camera_matrix);
    SetTransMatrix(camera_matrix);
    RotTrans(&g_camera_integer_coords, (VECTOR*)g_map_camera_matrix.t, &result);
    ScaleMatrix(&g_map_camera_matrix, &g_battle_map_camera_zoom);

    *camera_matrix = g_map_camera_matrix;
    g_battle_map_mesh_parts[0].matrix = g_map_camera_matrix;
    RotMatrix(&g_battle_map_light_rotation, &g_battle_map_light_rotation_matrix);
}

/* Run a weather display command.
 *
 * APPLY_SCRIPT_WEATHER reads EVENT_SCRIPT_VAR_WEATHER and issues the clear
 * command for no weather, or the set-up command for rain, storm or strong
 * storm, taking the snow variant on snowy maps; each choice also sets the
 * weather sounds and restores the lighting. The weather local is an s8: compares on
 * the promoted value copy it into a scratch register for the 3 and 4 tests,
 * as the target does. */
void weather_command_dispatch(u8 command) {
    s8 weather;

    switch (command) {
    case BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING:
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
        SetColorMatrix(g_weather_lightning_state.color_matrices);
        SetBackColor(g_battle_map_back_color.red, g_battle_map_back_color.green, g_battle_map_back_color.blue);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER:
        weather = script_variable_get(EVENT_SCRIPT_VAR_WEATHER);
        g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_OFF;
        weather &= 7;
        if (weather < WEATHER_RAIN) {
            weather_sfx_stop();
            weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_CLEAR);
            weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_RAIN) {
            if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                weather_sfx_stop();
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_LIGHT_SNOW);
            } else {
                weather_sfx_stop();
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_LIGHT_RAIN);
            }
            weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_STORM) {
            if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                weather_sfx_stop();
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_SNOW);
            } else {
                g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
                if (g_weather_sfx_enabled != 0) {
                    sound_background_play(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
                }
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RAIN);
            }
            weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_STRONG_STORM) {
            if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                weather_sfx_stop();
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_HEAVY_SNOW);
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
            } else {
                weather_sfx_stop();
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_HEAVY_RAIN);
                weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_LIGHTNING);
                g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
                if (g_weather_sfx_enabled != 0) {
                    sound_background_play(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
                }
            }
        }
        break;
    /* Both lightning commands zero color matrix 1, so on the flash's flicker
     * frames only the blue back color lights the map. */
    case BATTLE_MAP_WEATHER_COMMAND_THUNDER:
        g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_ONCE;
        g_weather_lightning_state.timer = 0x3c;
        g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
        if (g_weather_sfx_enabled != 0) {
            sound_background_play(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
        }
        g_weather_lightning_state.color_matrices[1].m[0][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[0][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[0][2] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][2] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][2] = 0;
        weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_LIGHTNING:
        g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_RANDOM;
        g_weather_lightning_state.timer = 0x3c;
        g_weather_lightning_state.color_matrices[1].m[0][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[0][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[0][2] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[1][2] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][0] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][1] = 0;
        g_weather_lightning_state.color_matrices[1].m[2][2] = 0;
        weather_command_dispatch(BATTLE_MAP_WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_SNOW:
        g_weather_fall_speed = 1;
        g_weather_primary_speed = 4;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_LIGHT_SNOW:
        g_weather_fall_speed = 1;
        g_weather_primary_speed = 1;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_HEAVY_SNOW:
        g_weather_fall_speed = 3;
        g_weather_primary_speed = 0x10;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_RAIN:
        g_weather_fall_speed = 5;
        g_weather_primary_speed = 6;
        g_weather_rain_secondary_fall_speed = 9;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_LIGHT_RAIN:
        g_weather_fall_speed = 3;
        g_weather_primary_speed = 5;
        g_weather_rain_secondary_fall_speed = 8;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_HEAVY_RAIN:
        g_weather_fall_speed = 5;
        g_weather_primary_speed = 9;
        g_weather_rain_secondary_fall_speed = 0x12;
        weather_particles_init(BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case BATTLE_MAP_WEATHER_COMMAND_CLEAR:
        g_weather_effect_mode = BATTLE_MAP_WEATHER_EFFECT_CLEAR;
        g_weather_lightning_mode = BATTLE_MAP_LIGHTNING_OFF;
        break;
    }
}

/* Set up the weather particles and their flat-textured sprites.
 *
 * INIT_SNOW_PARTICLES (from the SNOW, LIGHT_SNOW and HEAVY_SNOW commands)
 * scatters 64 flakes around the origin; INIT_RAIN_PARTICLES (from RAIN,
 * LIGHT_RAIN and HEAVY_RAIN) scatters 32 streak end pairs over the map extent
 * and records the lowest tile surface (largest y) in
 * g_battle_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
static void weather_particles_init(weather_command_e command) {
    RECT rects[3] = { { 16, 160, 15, 15 }, { 32, 160, 15, 15 }, { 48, 160, 15, 15 } };
    s16 sprite;
    s16 tile;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    switch (command & 0xff) {
    case BATTLE_MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES:
        g_weather_effect_mode = BATTLE_MAP_WEATHER_EFFECT_SNOW;
        for (i = 0; i < 64; i++) {
            g_weather_particles[i].y = -rand() % 240 - 16;
            g_weather_particles[i].fall_speed = g_weather_fall_speed;
            sprite = rand() % 3;
            for (j = 0; j < 2; j++) {
                SetSemiTrans(&g_render_buffers[j].ft3[i], 0);
                g_render_buffers[j].ft3[i].r0 = 0x80;
                g_render_buffers[j].ft3[i].g0 = 0x80;
                g_render_buffers[j].ft3[i].b0 = 0x80;
                g_render_buffers[j].ft3[i].clut = GetClut(0, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_render_buffers[j].ft3[i].u0 = rects[sprite].x;
                g_render_buffers[j].ft3[i].u1 = rects[sprite].x + rects[sprite].w;
                g_render_buffers[j].ft3[i].u2 = rects[sprite].x + rects[sprite].w;
                g_render_buffers[j].ft3[i].v0 = rects[sprite].y;
                g_render_buffers[j].ft3[i].v1 = rects[sprite].y;
                g_render_buffers[j].ft3[i].v2 = rects[sprite].y + rects[sprite].h;
            }
            g_weather_particles[i].fall_speed = sprite + g_weather_fall_speed;
            if (rand() & 1) {
                g_weather_particles[i].x = rand() % 140;
            } else {
                g_weather_particles[i].x = -(rand() % 140);
            }
            if (rand() & 1) {
                g_weather_particles[i].z = rand() % 140;
            } else {
                g_weather_particles[i].z = -(rand() % 140);
            }
        }
        break;
    case BATTLE_MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES:
        g_weather_effect_mode = BATTLE_MAP_WEATHER_EFFECT_RAIN;
        for (i = 0; i < 32; i++) {
            g_weather_particles[i].x = rand() % (g_battle_map_tile_width * BATTLE_MAP_TILE_SIZE);
            g_weather_particles[i].z = rand() % (g_battle_map_tile_depth * BATTLE_MAP_TILE_SIZE);
            g_weather_particles[i].y = -(rand() % 384) - 384;
            g_weather_particles[i + 32].x = g_weather_particles[i].x;
            g_weather_particles[i + 32].z = g_weather_particles[i].z;
            g_weather_particles[i + 32].y = g_weather_particles[i].y - rand() % 48;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < 32; i++) {
                SetSemiTrans(&g_render_buffers[j].ft3[i], 1);
                g_render_buffers[j].ft3[i].r0 = 0x64;
                g_render_buffers[j].ft3[i].g0 = 0x64;
                g_render_buffers[j].ft3[i].b0 = 0x64;
                g_render_buffers[j].ft3[i].clut = GetClut(0, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                prim = (POLY_FT3*)((u8*)g_render_buffers[0].ft3 + (j * sizeof(render_buffer_t) + i * sizeof(POLY_FT3)));
                setUV3(prim, 0xd8, 0xaf, 0xf7, 0xaf, 0xd8, 0xaf);
                SetSemiTrans(&g_render_buffers[j].ft3[i + 32], 1);
                g_render_buffers[j].ft3[i + 32].r0 = 0x80;
                g_render_buffers[j].ft3[i + 32].g0 = 0x80;
                g_render_buffers[j].ft3[i + 32].b0 = 0x80;
                g_render_buffers[j].ft3[i + 32].clut = GetClut(0, BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i + 32].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_render_buffers[j].ft3[i + 32].u0 = 0x10;
                g_render_buffers[j].ft3[i + 32].u1 = 0x1f;
                g_render_buffers[j].ft3[i + 32].v0 = 0xa0;
                g_render_buffers[j].ft3[i + 32].u2 = 0x10;
                g_render_buffers[j].ft3[i + 32].v1 = 0xaf;
                g_render_buffers[j].ft3[i + 32].v2 = 0xaf;
            }
        }
        g_battle_map_lowest_surface_y = 0x80000000;
        g_battle_map_tile_data_ptr = g_battle_map_tile_data;
        for (i = 0; i < g_battle_map_tile_width; i++) {
            for (j = 0; j < g_battle_map_tile_depth; j++) {
                tile = i + j * g_battle_map_tile_width;
                surface_y = -((g_battle_map_tile_data_ptr[tile].height
                                  + (g_battle_map_tile_data_ptr[tile].depth_half_height.depth))
                    * BATTLE_MAP_HEIGHT_STEP);
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
s32 battle_map_effect_data_state_set(battle_map_effect_data_command_e command) {
    s32 result;

    switch (command) {
    case BATTLE_MAP_EFFECT_DATA_END:
        result = 4;
        g_battle_map_untextured_otz_mask = 0;
        g_battle_map_untextured_fixed_ot_index = result;
        break;
    case BATTLE_MAP_EFFECT_DATA_BEGIN:
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
 * battle_map_mesh_weather_draw.
 *
 * A part whose running instruction row has a successor (states[0][1].next) adds
 * (speed >> 4) * g_system_vsync_interval to its elapsed time. When that
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
static void mesh_animations_update(void) {
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
            g_battle_map_mesh_parts[i].elapsed += (g_battle_map_mesh_parts[i].speed >> 4) * g_system_vsync_interval;
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
                *start_b = current_b[i * (sizeof(battle_map_mesh_part_t) / sizeof(SVECTOR))];
                g_battle_map_mesh_parts[i].elapsed = 1;

                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vx
                        - g_battle_map_mesh_parts[i].rotation_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vy
                        - g_battle_map_mesh_parts[i].rotation_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].rotation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vz
                        - g_battle_map_mesh_parts[i].rotation_start.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vx
                        - g_battle_map_mesh_parts[i].translation_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vy
                        - g_battle_map_mesh_parts[i].translation_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].translation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vz
                        - g_battle_map_mesh_parts[i].translation_start.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vx = g_battle_map_mesh_animation_keyframes[anim].scale.vx
                        - g_battle_map_mesh_parts[i].scale_start.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vy = g_battle_map_mesh_animation_keyframes[anim].scale.vy
                        - g_battle_map_mesh_parts[i].scale_start.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                    & BATTLE_MAP_MESH_TWEEN_TO) {
                    g_battle_map_mesh_parts[i].scale_step.vz = g_battle_map_mesh_animation_keyframes[anim].scale.vz
                        - g_battle_map_mesh_parts[i].scale_start.vz;
                }

                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].rotation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].rotation.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vx
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vy
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].translation_step.vz
                        = g_battle_map_mesh_animation_keyframes[anim].translation.vz;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vx = g_battle_map_mesh_animation_keyframes[anim].scale.vx;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vy = g_battle_map_mesh_animation_keyframes[anim].scale.vy;
                }
                if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                    & BATTLE_MAP_MESH_TWEEN_BY) {
                    g_battle_map_mesh_parts[i].scale_step.vz = g_battle_map_mesh_animation_keyframes[anim].scale.vz;
                }
            }

            /* Evaluate each channel's delta at elapsed / duration on its
             * curve: linear, sine or raised sine. */
            anim = g_battle_map_mesh_parts[i].keyframe;
            duration = g_battle_map_mesh_parts[i].duration;
            time = g_battle_map_mesh_parts[i].elapsed;
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vx
                    = g_battle_map_mesh_parts[i].translation_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vy
                    = g_battle_map_mesh_parts[i].translation_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].translation.vz
                    = g_battle_map_mesh_parts[i].translation_step.vz * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy * time / duration;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                & BATTLE_MAP_MESH_TWEEN_LINEAR) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz * time / duration;
            }

            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx
                        * rsin(
                            (g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                                - g_battle_map_mesh_animation_keyframes[anim]
                                    .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_X])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_X])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy
                        * rsin(
                            (g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                                - g_battle_map_mesh_animation_keyframes[anim]
                                    .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz
                        * rsin(
                            (g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                                - g_battle_map_mesh_animation_keyframes[anim]
                                    .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vx = g_battle_map_mesh_parts[i].translation_step.vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vy = g_battle_map_mesh_parts[i].translation_step.vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].translation.vz = g_battle_map_mesh_parts[i].translation_step.vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim]
                                .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_X])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_X])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Y])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Y])
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                & BATTLE_MAP_MESH_TWEEN_SINE) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz
                        * rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                                   - g_battle_map_mesh_animation_keyframes[anim]
                                       .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Z])
                                * time / duration
                            + g_battle_map_mesh_animation_keyframes[anim].angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Z])
                    >> 12;
            }

            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vx = g_battle_map_mesh_parts[i].rotation_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_X]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_X])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_X])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vy = g_battle_map_mesh_parts[i].rotation_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Y])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].rotation.vz = g_battle_map_mesh_parts[i].rotation_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_ROTATION_Z])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vx = g_battle_map_mesh_parts[i].translation_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vy = g_battle_map_mesh_parts[i].translation_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].translation.vz = g_battle_map_mesh_parts[i].translation_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_end[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vx = g_battle_map_mesh_parts[i].scale_step.vx
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_X]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_X])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_X])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vy = g_battle_map_mesh_parts[i].scale_step.vy
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_Y]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Y])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Y])
                            + ONE)
                    >> 12;
            }
            if (g_battle_map_mesh_animation_keyframes[anim].tween_flags[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                & BATTLE_MAP_MESH_TWEEN_RAISED_SINE) {
                g_battle_map_mesh_parts[i].scale.vz = g_battle_map_mesh_parts[i].scale_step.vz
                        * (rsin((g_battle_map_mesh_animation_keyframes[anim].angle_end[BATTLE_MAP_MESH_CHANNEL_SCALE_Z]
                                    - g_battle_map_mesh_animation_keyframes[anim]
                                        .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Z])
                                   * time / duration
                               + g_battle_map_mesh_animation_keyframes[anim]
                                   .angle_start[BATTLE_MAP_MESH_CHANNEL_SCALE_Z])
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
 * duration. Only the two GET commands set the result; the others return the
 * unset local. */
s32 battle_map_data_command_dispatch(battle_map_data_command_e command, u32 index, s32 object_state, s32 flag) {
    s32 result;
    s16 i;
    mesh_instruction_list_t* entry;

    switch (command) {
    case BATTLE_MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE:
        result = g_battle_map_texture_animations[index].active;
        break;
    case BATTLE_MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_battle_map_texture_animations[index].mode == BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE) {
            if (g_saved_texture_modes[index] != 0) {
                g_battle_map_texture_animations[index].mode = g_saved_texture_modes[index];
            }
        }
        switch (g_battle_map_texture_animations[index].mode) {
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_LOOP ... BATTLE_MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK ... BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_texture_animation_moves[index].sx = g_battle_map_texture_animations[index].frame_x;
            g_texture_animation_moves[index].sy = g_battle_map_texture_animations[index].frame_y;
            g_texture_animation_moves[index].w = g_battle_map_texture_animations[index].width;
            g_texture_animation_moves[index].h = g_battle_map_texture_animations[index].height;
            g_texture_animation_moves[index].x0 = g_battle_map_texture_animations[index].canvas_x;
            g_texture_animation_moves[index].y0 = g_battle_map_texture_animations[index].canvas_y;
            g_battle_map_texture_animations[index].active = 1;
            g_battle_map_texture_animations[index].timer = 0x7e;
            g_battle_map_texture_animations[index].frame = 0;
            break;
        default:
            system_error_report(2, 0x3cd);
            break;
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_GET_3D_OBJECT_STATE:
        result = g_battle_map_mesh_part_animation_states[index];
        break;
    case BATTLE_MAP_DATA_COMMAND_SET_3D_OBJECT_STATE:
        entry = &g_battle_map_mesh_animation_instructions[index];
        for (i = 0; i < 17; i++) {
            entry->states[0][i] = entry->states[object_state][i];
        }
        g_battle_map_mesh_parts[index].elapsed = 2;
        g_battle_map_mesh_parts[index].duration = 0;
        g_battle_map_mesh_parts[index].next_instruction = 1;
        g_battle_map_mesh_part_animation_states[index] = (object_state << 8) | flag;
        break;
    case BATTLE_MAP_DATA_COMMAND_SPLIT_MESH_PARTS:
        /* Cut the primary mesh's polygons of each class into eight equal runs for
         * parts 1-8 (the last may run past the end) and leave part 0 empty. */
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
    case BATTLE_MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER:
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    case BATTLE_MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER:
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    case BATTLE_MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < BATTLE_MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_saved_texture_modes[i] = g_battle_map_texture_animations[i].mode;
            g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < BATTLE_MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_battle_map_texture_animations[i].mode = g_saved_texture_modes[i];
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < BATTLE_MAP_TEXTURE_ANIMATION_COUNT) {
            g_saved_texture_modes[index] = g_battle_map_texture_animations[index].mode;
            g_battle_map_texture_animations[index].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < BATTLE_MAP_TEXTURE_ANIMATION_COUNT) {
            g_battle_map_texture_animations[index].mode = g_saved_texture_modes[index];
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION:
        if (index < BATTLE_MAP_TEXTURE_ANIMATION_COUNT) {
            g_battle_map_texture_animations[index].duration = object_state;
            g_battle_map_texture_animations[index].timer = 0;
        }
        break;
    case BATTLE_MAP_DATA_COMMAND_RELEASE_GNS_HOLD:
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
 * Only the low byte of g_system_vsync_interval is read (lbu), and the start call
 * passes the color range unmasked, as the target does.
 *
 * The two loop modes count frames and step once the timer reaches the
 * duration; the other modes count vertical blanks and step once it exceeds it.
 */
static void texture_animations_update(void) {
    s32 i;

    for (i = 0; i < BATTLE_MAP_TEXTURE_ANIMATION_COUNT; i++) {
        if (g_battle_map_texture_animations[i].active == 0) {
            continue;
        }
        switch (g_battle_map_texture_animations[i].mode) {
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_LOOP:
            g_battle_map_texture_animations[i].timer++;
            if (g_battle_map_texture_animations[i].timer < g_battle_map_texture_animations[i].duration) {
                break;
            }

            g_battle_map_texture_animations[i].timer = 0;
            g_battle_map_texture_animations[i].frame++;
            g_battle_map_texture_animations[i].column++;
            /* Past the right edge of the 64-wide texture page, continue at the start of the next row. */
            if (g_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row++;
                g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2
                    - g_battle_map_texture_animations[i].width;
            }
            if (g_battle_map_texture_animations[i].frame_count - 1 < g_battle_map_texture_animations[i].frame) {
                g_battle_map_texture_animations[i].frame = 0;
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row = 0;
                g_texture_animation_moves[i].sx
                    = g_battle_map_texture_animations[i].frame_x - g_battle_map_texture_animations[i].width;
            }

            /* Aim the DR_MOVE at the new frame and queue its copy onto the canvas. */
            g_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
            g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
            AddPrim(g_render_current_buffer->otag, &g_texture_animation_moves[i]);
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2
                        - g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame_count - 2 < g_battle_map_texture_animations[i].frame) {
                    g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK;
                }

                g_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &g_texture_animation_moves[i]);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                /* Before the page's left edge, go back to the last column of the previous row. */
                if (g_texture_animation_moves[i].sx - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (g_battle_map_texture_animations[i].frame_x % 64))
                        / g_battle_map_texture_animations[i].width;
                    g_battle_map_texture_animations[i].row--;
                    g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x
                        + g_battle_map_texture_animations[i].column * g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG;
                }

                g_texture_animation_moves[i].sx -= g_battle_map_texture_animations[i].width;
                g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &g_texture_animation_moves[i]);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP:
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

            battle_map_palette_load_data(g_battle_map_palette_animation_frames
                    + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].column,
                BATTLE_MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count - 1) {
                    g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK;
                }
                battle_map_palette_load_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    BATTLE_MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG;
                }
                battle_map_palette_load_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    BATTLE_MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_1:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_2:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_3:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_4:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_5:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_6:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_7:
            /* Walk the packet to the last frame, then play backward from it. */
            g_battle_map_texture_animations[i].frame = 0;
            while (g_battle_map_texture_animations[i].frame < g_battle_map_texture_animations[i].frame_count) {
                g_battle_map_texture_animations[i].column++;
                if (g_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2
                        - g_battle_map_texture_animations[i].width;
                }
                g_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                g_battle_map_texture_animations[i].frame++;
            }

            g_battle_map_texture_animations[i].timer = 0x7E;
            g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING;
            /* fall through */
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                if (g_texture_animation_moves[i].sx - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (g_battle_map_texture_animations[i].frame_x % 64))
                            / g_battle_map_texture_animations[i].width
                        - 1;
                    g_battle_map_texture_animations[i].row--;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].active = 0;
                    g_battle_map_texture_animations[i].mode = BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0;
                }

                g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x
                    + g_battle_map_texture_animations[i].width * g_battle_map_texture_animations[i].column;
                g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &g_texture_animation_moves[i]);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_1:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_2:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_3:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_4:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_5:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_6:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_7:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                if (g_battle_map_texture_animations[i].column != 0) {
                    g_texture_animation_moves[i].sx += g_battle_map_texture_animations[i].width;
                }
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_texture_animation_moves[i].sx + g_battle_map_texture_animations[i].width
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    g_texture_animation_moves[i].sy
                        += g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                }

                if (g_battle_map_texture_animations[i].frame > g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].frame = 0;
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row = 0;
                    g_battle_map_texture_animations[i].active = 0;
                    g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x;
                    g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_render_current_buffer->otag, &g_texture_animation_moves[i]);
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_0:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_1:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_2:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_3:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_4:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_5:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_6:
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_7:
            g_battle_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                battle_map_palette_load_data(g_battle_map_palette_animation_frames
                        + g_battle_map_texture_animations[i].frame_x + g_battle_map_texture_animations[i].frame,
                    BATTLE_MAP_CLUT_ROW_MAP, g_battle_map_texture_animations[i].canvas_x / 16, 0);
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].active = 0;
                }
            }
            break;
        case BATTLE_MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
            /* Start the color rotation once; battle_map_update_color_animations runs it from then on. */
            if (g_battle_map_texture_animation_cycle_handles[i] == 0) {
                g_battle_map_texture_animation_cycle_handles[i]
                    = battle_map_clut_start_cycle(g_battle_map_texture_animations[i].duration, BATTLE_MAP_CLUT_ROW_MAP,
                        g_battle_map_texture_animations[i].canvas_x,
                        g_battle_map_texture_animations[i].canvas_x + g_battle_map_texture_animations[i].width - 1);
                /* All eight rotation slots are in use. */
                if (g_battle_map_texture_animation_cycle_handles[i] == 0) {
                    system_error_report(2, 0x3CC);
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
 * entry, extra entries skipped). The caller stops at type 0 before calling.
 * An unknown nonzero type leaves `next` unset (the target returns $a1), yet
 * the caller uses that result to read the next record's type. */
static battle_map_state_record_t* animation_record_load(battle_map_state_record_t* record) {
    battle_map_state_record_t* next;
    u32 count;
    u32 i;
    u32 y;
    u32 c;

    count = record->count;
    switch (record->type) {
    case BATTLE_MAP_MESH_ANIMATION_RECORD_KEYFRAMES: {
        battle_map_mesh_keyframe_t* src = (battle_map_mesh_keyframe_t*)record->data;

        count++;
        for (i = 1; i < count; i++) {
            g_battle_map_mesh_animation_keyframes[i] = *src++;
        }
        next = (battle_map_state_record_t*)src;
        break;
    }
    case BATTLE_MAP_MESH_ANIMATION_RECORD_END:
        break;
    case BATTLE_MAP_MESH_ANIMATION_RECORD_INSTRUCTIONS: {
        mesh_instruction_t* src = (mesh_instruction_t*)record->data;

        for (y = 0; y < 8; y++) {
            for (i = 1; i < 9; i++) {
                for (c = 1; c < 17; c++) {
                    g_battle_map_mesh_animation_instructions[i].states[y][c] = *src++;
                }
            }
        }
        next = (battle_map_state_record_t*)src;
        break;
    }
    case BATTLE_MAP_MESH_ANIMATION_RECORD_PART_PROPERTIES: {
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
                          .scale.vx;
            g_battle_map_mesh_parts[i].scale.vy
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .scale.vy;
            g_battle_map_mesh_parts[i].scale.vz
                = g_battle_map_mesh_animation_keyframes
                      [g_battle_map_mesh_animation_instructions[i].states[0][1].keyframe]
                          .scale.vz;
        }
        for (; i < count; i++) {
            src += 4;
        }
        next = (battle_map_state_record_t*)src;
        break;
    }
    }
    return next;
}

/* 1 when the requested map state stands in the relation to a GNS row's state that
 * the row's comparison byte asks for: equal (1), requested less than the row's
 * (2) or requested greater (3). Any other comparison never matches. Every file
 * row of the retail GNS files uses 1. */
s16 battle_map_gns_record_matches_state(s32 comparison, s16 record_state, s16 requested_state) {
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
s32 battle_map_gns_resource_dispatch(u8 resource_type, battle_map_resource_t* data) {
    MATRIX direction;
    SVECTOR zero_vector;
    VECTOR unit_scale;
    battle_map_texture_animation_t* source;
    battle_map_light_resource_t* light;
    u16* polygon_flags;
    /* 16-bit cursor of the light, camera and weather-modifier cases.
     * Direct command payload-vector access removes four bytes from each arm. */
    s16* vector;
    battle_map_state_record_t* cursor;
    s32 result;
    s32 i;
    u16 flags;
    u8 red;
    u8 green;
    u8 blue;

    zero_vector = g_zero_svector;
    unit_scale = g_unit_scale_vector;

    /* Let queued drawing finish before a resource replaces data the GPU may still be reading. */
    while (DrawSync(1) != 0) { }
    switch (resource_type) {
    case BATTLE_MAP_RESOURCE_GEOMETRY_PRIMARY:
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
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[0]);

        /* The counts after part 0 alone are kept as the primary mesh's. */
        g_battle_map_primary_textured_triangle_count = g_battle_map_textured_triangle_count;
        g_battle_map_primary_textured_quad_count = g_battle_map_textured_quad_count;
        g_battle_map_primary_untextured_triangle_count = g_battle_map_untextured_triangle_count;
        g_battle_map_primary_untextured_quad_count = g_battle_map_untextured_quad_count;
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_1:
        g_battle_map_mesh_part_geometry[1] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[1]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_2:
        g_battle_map_mesh_part_geometry[2] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[2]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_3:
        g_battle_map_mesh_part_geometry[3] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[3]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_4:
        g_battle_map_mesh_part_geometry[4] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[4]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_5:
        g_battle_map_mesh_part_geometry[5] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[5]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_6:
        g_battle_map_mesh_part_geometry[6] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[6]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_7:
        g_battle_map_mesh_part_geometry[7] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[7]);
        break;
    case BATTLE_MAP_RESOURCE_GEOMETRY_PART_8:
        g_battle_map_mesh_part_geometry[8] = &data->geometry;
        battle_map_mesh_append_geometry(&data->geometry, &g_battle_map_mesh_parts[8]);
        break;
    case BATTLE_MAP_RESOURCE_POLYGON_FLAGS:
        polygon_flags = data->polygon_flags.textured_triangles;
        for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
            flags = *polygon_flags++;
            g_battle_map_textured_triangle_positions[i].polygon_flags = flags;
            /* An unlit polygon starts at the neutral color 0x80 in both render buffers. */
            if (!(flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                g_render_buffers[0].gt3[i].r0 = 0x80;
                g_render_buffers[0].gt3[i].r1 = 0x80;
                g_render_buffers[0].gt3[i].r2 = 0x80;
                g_render_buffers[0].gt3[i].g0 = 0x80;
                g_render_buffers[0].gt3[i].g1 = 0x80;
                g_render_buffers[0].gt3[i].g2 = 0x80;
                g_render_buffers[0].gt3[i].b0 = 0x80;
                g_render_buffers[0].gt3[i].b1 = 0x80;
                g_render_buffers[0].gt3[i].b2 = 0x80;
                g_render_buffers[1].gt3[i].r0 = 0x80;
                g_render_buffers[1].gt3[i].r1 = 0x80;
                g_render_buffers[1].gt3[i].r2 = 0x80;
                g_render_buffers[1].gt3[i].g0 = 0x80;
                g_render_buffers[1].gt3[i].g1 = 0x80;
                g_render_buffers[1].gt3[i].g2 = 0x80;
                g_render_buffers[1].gt3[i].b0 = 0x80;
                g_render_buffers[1].gt3[i].b1 = 0x80;
                g_render_buffers[1].gt3[i].b2 = 0x80;
            }
        }

        /* The file reserves 0x200 triangle and 0x300 quad entries; skip those past the 360 and 710 used. */
        for (; i < 0x200; i++) {
            polygon_flags++;
        }

        for (i = 0; i < RENDER_TEXTURED_QUAD_COUNT; i++) {
            flags = *polygon_flags++;
            g_battle_map_textured_quad_positions[i].polygon_flags = flags;
            if (!(flags & BATTLE_MAP_POLYGON_FLAG_LIT)) {
                g_render_buffers[0].gt4[i].r0 = 0x80;
                g_render_buffers[0].gt4[i].r1 = 0x80;
                g_render_buffers[0].gt4[i].r2 = 0x80;
                g_render_buffers[0].gt4[i].r3 = 0x80;
                g_render_buffers[0].gt4[i].g0 = 0x80;
                g_render_buffers[0].gt4[i].g1 = 0x80;
                g_render_buffers[0].gt4[i].g2 = 0x80;
                g_render_buffers[0].gt4[i].g3 = 0x80;
                g_render_buffers[0].gt4[i].b0 = 0x80;
                g_render_buffers[0].gt4[i].b1 = 0x80;
                g_render_buffers[0].gt4[i].b2 = 0x80;
                g_render_buffers[0].gt4[i].b3 = 0x80;
                g_render_buffers[1].gt4[i].r0 = 0x80;
                g_render_buffers[1].gt4[i].r1 = 0x80;
                g_render_buffers[1].gt4[i].r2 = 0x80;
                g_render_buffers[1].gt4[i].r3 = 0x80;
                g_render_buffers[1].gt4[i].g0 = 0x80;
                g_render_buffers[1].gt4[i].g1 = 0x80;
                g_render_buffers[1].gt4[i].g2 = 0x80;
                g_render_buffers[1].gt4[i].g3 = 0x80;
                g_render_buffers[1].gt4[i].b0 = 0x80;
                g_render_buffers[1].gt4[i].b1 = 0x80;
                g_render_buffers[1].gt4[i].b2 = 0x80;
                g_render_buffers[1].gt4[i].b3 = 0x80;
            }
        }

        for (; i < 0x300; i++) {
            polygon_flags++;
        }

        for (i = 0; i < RENDER_UNTEXTURED_TRIANGLE_COUNT; i++) {
            g_battle_map_untextured_triangle_positions[i].polygon_flags = *polygon_flags++;
        }
        for (i = 0; i < RENDER_UNTEXTURED_QUAD_COUNT; i++) {
            g_battle_map_untextured_quad_positions[i].polygon_flags = *polygon_flags++;
        }

        polygon_flags_dispatch(BATTLE_MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        break;
    case BATTLE_MAP_RESOURCE_PALETTES:
        g_battle_map_palette_pending = 1;
        for (i = 0; i < 0x200; i++) {
            g_battle_map_pending_palette_data[i] = data->bytes[i];
        }
        break;
    case BATTLE_MAP_RESOURCE_APPLY_PALETTES:
        g_battle_map_vram_load_rect.y = BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_MAP;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_palette_pending = 0;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_palette_load_data(data->clut, BATTLE_MAP_CLUT_ROW_MAP, 0, 1);
        break;
    case BATTLE_MAP_RESOURCE_MARKED_TILE_PALETTES:
        g_battle_map_vram_load_rect.y = BATTLE_MAP_CLUT_ROWS_Y + BATTLE_MAP_CLUT_ROW_MARKED_TILES;
        g_battle_map_vram_load_rect.w = 0x100;
        g_battle_map_vram_load_rect.x = 0;
        g_battle_map_vram_load_rect.h = 1;
        battle_map_palette_load_data(data->clut, BATTLE_MAP_CLUT_ROW_MARKED_TILES, 0, 1);
        break;
    case BATTLE_MAP_RESOURCE_UNSUPPORTED_12:
        system_error_report(2, 0x3ca);
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_0:
        g_battle_map_vram_load_rect.x = 0x300;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_1:
        g_battle_map_vram_load_rect.x = 0x340;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_2:
        g_battle_map_vram_load_rect.x = 0x380;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_3:
        g_battle_map_vram_load_rect.x = 0x3c0;
        g_battle_map_vram_load_rect.w = 0x40;
        g_battle_map_vram_load_rect.y = 0;
        g_battle_map_vram_load_rect.h = 0x100;
        LoadImage(&g_battle_map_vram_load_rect, data->texture_words);
        while (DrawSync(1) != 0) { }
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE:
        /* A full texture is four 0x8000-byte quarters, uploaded side by side from VRAM x 0x300. */
        if (g_battle_map_texture_quarter_0_pending != 0) {
            battle_map_gns_resource_dispatch(BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_0, data);
        }
        if (g_battle_map_texture_quarter_1_pending != 0) {
            battle_map_gns_resource_dispatch(
                BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_1, (battle_map_resource_t*)(data->bytes + 0x8000));
        }
        if (g_battle_map_texture_quarter_2_pending != 0) {
            battle_map_gns_resource_dispatch(
                BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_2, (battle_map_resource_t*)(data->bytes + 0x10000));
        }
        if (g_battle_map_texture_quarter_3_pending != 0) {
            battle_map_gns_resource_dispatch(
                BATTLE_MAP_RESOURCE_TEXTURE_QUARTER_3, (battle_map_resource_t*)(data->bytes + 0x18000));
        }

        g_battle_map_texture_quarter_0_pending = 0;
        g_battle_map_texture_quarter_1_pending = 0;
        g_battle_map_texture_quarter_2_pending = 0;
        g_battle_map_texture_quarter_3_pending = 0;
        g_battle_map_texture_pending = 0;
        break;
    case BATTLE_MAP_RESOURCE_MESH_PRIMARY:
    case BATTLE_MAP_RESOURCE_MESH_OVERRIDE:
    case BATTLE_MAP_RESOURCE_MESH_ALTERNATE:
        for (i = BATTLE_MAP_RESOURCE_GEOMETRY_PRIMARY; i < GNS_RESOURCE_TABLE_COUNT; i++) {
            if (data->mesh_file[i] != 0) {
                battle_map_gns_resource_dispatch(i, (battle_map_resource_t*)(data->bytes + data->mesh_file[i]));
            }
        }
        break;
    case BATTLE_MAP_RESOURCE_LIGHTING:
        g_battle_map_lighting_pending = 1;
        for (i = 0; i < 45; i++) {
            g_battle_map_pending_lighting_data[i] = data->bytes[i];
        }
        break;
    case BATTLE_MAP_RESOURCE_APPLY_LIGHTING:
        light = &data->lighting;
        g_battle_map_lighting_pending = 0;
        setVector(&g_battle_map_light_rotation, 0, 0, 0);

        /* Start at the color matrix to retain the target's separate 18-byte address step. */
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

        /* Retail reads packed direction bytes as the first MATRIX translation fields
         * and copies the uninitialized direction.t fields of the second matrix. */
        light_matrices_set((MATRIX*)light, &direction);
        battle_map_light_control_back_color(
            BATTLE_MAP_LIGHT_BACK_COLOR_SET, light->ambient.r, light->ambient.g, light->ambient.b);

        red = light->gradient_top.r;
        green = light->gradient_top.g;
        blue = light->gradient_top.b;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].r = red;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].g = green;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP].b = blue;
        red = light->gradient_bottom.r;
        green = light->gradient_bottom.g;
        blue = light->gradient_bottom.b;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].r = red;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].g = green;
        g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM].b = blue;

        /* Lightning flashes fade back to these colors. */
        g_weather_lightning_state.base_colors[0]
            = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_TOP];
        g_weather_lightning_state.base_colors[1]
            = g_battle_map_background_gradient_colors.colors[BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM];
        battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
        break;
    case BATTLE_MAP_RESOURCE_TEXTURE_ANIMATIONS:
        source = data->texture_animations;
        for (i = 0; i < BATTLE_MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_battle_map_texture_animation_cycle_handles[i] = 0;
            g_battle_map_texture_animations[i] = source[i];
            SetDrawMove(&g_texture_animation_moves[i]);
            g_texture_animation_moves[i].sx = g_battle_map_texture_animations[i].frame_x;
            g_texture_animation_moves[i].sy = g_battle_map_texture_animations[i].frame_y;
            g_texture_animation_moves[i].w = g_battle_map_texture_animations[i].width;
            g_texture_animation_moves[i].h = g_battle_map_texture_animations[i].height;
            g_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].canvas_x;
            g_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].canvas_y;
            /* Play-once modes wait for battle_map_data_command_dispatch to start them; the rest run at once. */
            if (g_battle_map_texture_animations[i].mode >= BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0
                && g_battle_map_texture_animations[i].mode < BATTLE_MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE) {
                g_battle_map_texture_animations[i].active = 0;
            } else if (g_battle_map_texture_animations[i].mode != BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE) {
                g_battle_map_texture_animations[i].active = 1;
            }
        }
        break;
    case BATTLE_MAP_RESOURCE_PALETTE_ANIMATION_FRAMES:
        for (i = 0; i < 0x100; i++) {
            /* Palette-bank indexing changes the native 16-bit copy loop. */
            ((u16*)g_battle_map_palette_animation_frames)[i] = ((u16*)data->clut)[i];
        }
        break;
    case BATTLE_MAP_RESOURCE_TERRAIN: {
        battle_map_terrain_resource_t* terrain = &data->terrain;
        g_battle_map_tile_width = terrain->width;
        g_battle_map_tile_depth = terrain->depth;
        battle_map_tile_data_copy(data->bytes);
        break;
    }
    case BATTLE_MAP_RESOURCE_STATE_RECORDS:
        cursor = &data->state_record;
        if (cursor->type != BATTLE_MAP_MESH_ANIMATION_RECORD_END) {
            do {
                cursor = animation_record_load(cursor);
            } while (cursor->type != BATTLE_MAP_MESH_ANIMATION_RECORD_END);
        }
        break;
    case GNS_COMMAND_SET_CAMERA_POSITION:
        /* Halfword 6 is the payload vector at +0x0c. */
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_camera_integer_coords, -vector[0], vector[1], -vector[2]);
        camera_cursor_focus_set(&g_camera_integer_coords, &g_battle_map_camera_rotation);
        break;
    case GNS_COMMAND_SET_CAMERA_ROTATION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_battle_map_camera_rotation, vector[0], vector[1], vector[2]);
        camera_cursor_focus_set(&g_camera_integer_coords, &g_battle_map_camera_rotation);
        break;
    /* Case-local pointers preserve the native address lifetimes. */
    case GNS_COMMAND_SET_INDOOR: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags |= BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER;
        /* Indoors the backdrop gradient is drawn without dithering. */
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    }
    case GNS_COMMAND_SET_OUTDOOR: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags &= ~BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    }
    case GNS_COMMAND_CLEAR_SNOW: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags &= ~BATTLE_MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_SNOW: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags |= BATTLE_MAP_WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_WEATHER_MODIFIER: {
        s32* weather_flags;

        vector = data->command_halfwords;
        vector += 6;
        weather_flags = &g_weather_flags;
        *weather_flags
            = (*weather_flags & BATTLE_MAP_WEATHER_LOW_FLAGS_MASK) | (vector[0] << BATTLE_MAP_WEATHER_MODIFIER_SHIFT);
        break;
    }
    }
    return result;
}

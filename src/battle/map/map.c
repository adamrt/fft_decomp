/* The battle map, BATTLE's map image. A map's GNS file lists its resources (meshes, textures, palettes, lighting,
 * terrain and animations) by layout, weather and time of day; the loader reads the ones the scenario selects and
 * applies them. Each frame the map draws its mesh parts, backdrop, weather particles and tile marks, and runs the
 * lighting, weather, texture and mesh-part animation commands that events and effects issue. The image is one object
 * in the original: its read-only data runs unbroken, and the loader shares its constants with the render state. */

#include "fft/map/map.h"
#include "fft/battle/camera.h"
#include "fft/battle/move.h"
#include "fft/battle/weather.h"
#include "fft/effect/effect.h"
#include "fft/gfx/gfx.h"
#include "fft/map/map_light.h"
#include "fft/map/map_load.h"
#include "fft/map/map_mesh.h"
#include "fft/map/map_palette.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/system.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The four primitive queues have no C source: hand-written MIPS in the original; the GTE routines are kept as original
 * bytes through the `regions:` rows of target/battle.yaml (each returns from its prologue and keeps its body after the
 * jr ra); a port supplies the bodies. */
void map_queue_textured_quads(u32* otag, POLY_GT4* prims, s32 yaw, s32* unused_count);
void map_queue_textured_triangles(u32* otag, POLY_GT3* prims, s32 yaw, s32* unused_count);
void map_queue_untextured_quads(u32* otag, POLY_F4* prims, s32 yaw, s32* unused_count);
void map_queue_untextured_triangles(u32* otag, POLY_F3* prims, s32 yaw, s32* unused_count);

/* Weather particles of g_weather_particles: snow
 * (WEATHER_COMMAND_INIT_SNOW_PARTICLES) uses all 64, rain
 * (WEATHER_COMMAND_INIT_RAIN_PARTICLES) fills two sets of 32. */
typedef struct weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* s_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} weather_particle_t;
typedef char weather_particle_size_must_be_8[sizeof(weather_particle_t) == 8 ? 1 : -1];

/* Lightning flash state (s_weather_lightning_state).
 * set_light_matrices fills color_matrices[0] and [2]; the lightning weather commands
 * clear the rotation part of color_matrices[1] and start the 60-frame flash timer. */
typedef struct lightning_state {
    u32 timer;                                                /* 0x00 */
    MATRIX color_matrices[3];                                 /* 0x04 */
    u8 _unused_64[0x20];                                      /* 0x64 */
    CVECTOR base_colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT]; /* 0x84 */
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];      /* 0x8c */
} lightning_state_t;
typedef char lightning_state_size_must_be_148[(sizeof(lightning_state_t) == 148) ? 1 : -1];

/* Values of s_weather_lightning_mode. */
typedef enum lightning_mode {
    MAP_LIGHTNING_OFF = 0,
    MAP_LIGHTNING_RANDOM = 1,
    MAP_LIGHTNING_ONCE = 2 /* back to OFF when the flash ends */
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
extern s32 g_map_lowest_surface_y;
extern map_tile_t* g_map_tile_data_ptr;
extern u8 g_map_tile_depth;
extern u8 g_map_tile_width;
extern u16 g_map_tile_mark_x;
extern s16 g_map_tile_mark_y;
extern map_tile_mark_layers_t g_map_tile_mark_modes;

/* map_tile_mark_mode_e, one per tile mark */

/* Weather particles and their transforms. */
extern weather_particle_t g_weather_particles[64];
extern weather_particle_t g_weather_rain_splash_particles[32];
extern u8 g_weather_primary_speed;
extern u8 g_weather_rain_secondary_fall_speed;
extern MATRIX g_weather_matrix;
extern SVECTOR g_weather_rotation;
extern VECTOR g_weather_scale;
extern s32 g_weather_sprite_scale;
extern VECTOR g_weather_translation;
extern SVECTOR g_weather_timed_transform_rotation;
extern s16 g_weather_effect_mode; /* weather_effect_mode_e */

/* Frame and duration of E458's timed particle transform (WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM). */
extern s32 g_weather_timed_transform_duration;

/* Sound IDs remembered so the weather sounds can be stopped; zero means no ID is stored.
 * The rain ID is also retained while weather sounds are disabled. */
extern s32 g_weather_rain_sfx_id;
extern s32 g_weather_thunder_a_sfx_id;
extern s32 g_weather_thunder_b_sfx_id;
extern u8 g_weather_sfx_enabled;

/* weather_start_timed_transform stores 0x95; nothing reads it. */
extern s32 g_weather_timed_transform_unread;

/* Palette animation upload state. */
extern clut_t g_map_palette_animation_frames;
extern RECT g_map_vram_load_rect;

/* Working, saved and interpolated lighting matrices. */
extern MATRIX g_map_light_color_matrix;
extern MATRIX g_map_light_matrix;
extern SVECTOR g_map_light_rotation;
extern MATRIX g_map_light_rotation_matrix;
extern MATRIX g_map_saved_light_color_matrix;            /* saved color matrix (command 0x5f/0x60) */
extern MATRIX g_map_saved_light_matrix;                  /* saved light matrix (command 0x5f/0x60) */
extern u16 g_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_map_light_transition_start_color_matrix; /* transition start color matrix */
extern MATRIX g_map_light_transition_start_light_matrix; /* transition start light matrix */

/* The transition's target (command 0x66) or delta (0x68) color and light matrices. */
extern MATRIX g_map_light_transition_target_color_matrix;
extern MATRIX g_map_light_transition_target_light_matrix;

/* The applied background gradient pair. The copies in map_light_dispatch_command
 * move each color as an independent unaligned 32-bit value (lwl/lwr + swl/swr). */
extern map_background_gradient_colors_t g_map_background_gradient_colors;
extern CVECTOR g_map_saved_background_gradient_first;
extern CVECTOR g_map_saved_background_gradient_second;
static rgb24_t s_map_ambient_polygon_color;
extern render_buffer_t* g_map_ambient_restored_buffer;
extern CVECTOR g_map_current_darkness_color;
extern u8 g_map_back_color_disabled;
extern CVECTOR g_map_cached_back_color;
extern u8 g_map_polygon_flag_clear_countdown;

/* Mesh animation streams and the currently selected part. */
extern mesh_instruction_list_t g_map_mesh_animation_instructions[9];
extern s32 g_map_mesh_part_animation_states[9];
extern map_mesh_part_t g_map_mesh_parts[9];
extern map_texture_animation_t g_map_texture_animations[MAP_TEXTURE_ANIMATION_COUNT];
extern s32 g_map_primary_textured_quad_count;
extern s32 g_map_primary_textured_triangle_count;
extern s32 g_map_primary_untextured_quad_count;
extern s32 g_map_primary_untextured_triangle_count;
extern map_mesh_triangle_positions_t* g_map_part_textured_triangle_positions;
extern map_mesh_quad_positions_t* g_map_part_textured_quad_positions;
extern map_mesh_triangle_normals_t* g_map_part_textured_triangle_normals;
extern map_mesh_quad_normals_t* g_map_part_textured_quad_normals;
extern s32 g_map_part_textured_quad_count;
extern s32 g_map_part_textured_triangle_count;
extern s32 g_map_part_untextured_quad_count;
extern s32 g_map_part_untextured_triangle_count;

/* Map projection state. */
extern VECTOR g_map_camera_zoom;
extern SVECTOR g_map_camera_rotation;

/* Untextured map polygons go to OT slot (otz & mask) | fixed index: {0, 4}
 * normally, {-1, 0} while map effect data is active (command 0x88), so a
 * non-zero mask also marks that state. */
extern s32 g_map_untextured_fixed_ot_index;
extern s32 g_map_untextured_otz_mask;

/* Default rotation and scale copied when mesh parts or map resources are initialized. */
static const SVECTOR s_zero_svector;

static const VECTOR s_unit_scale_vector;

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

/* g_map_resource_load_state; these are not the loader's normal return values. */
typedef enum resource_load_state {
    MAP_LOAD_STATE_IDLE = 0x00,
    MAP_LOAD_STATE_GNS_FILE = 0x74, /* also accepted as a no-op input command */
    MAP_LOAD_STATE_DONE = 0x77,
    MAP_LOAD_STATE_RESOURCE_FILE = 0x78,
    MAP_LOAD_STATE_MESH_RECORDS = 0x79, /* rebuild render records once per buffer */
    MAP_LOAD_STATE_MESH_RECORDS_SECOND = 0x91,
    MAP_LOAD_STATE_APPLY_LIGHTING = 0x92,
    MAP_LOAD_STATE_APPLY_TEXTURE = 0x93
} resource_load_state_e;

/* g_map_gns_load_phase and the normal return of map_update_load. */
typedef enum gns_load_phase {
    MAP_GNS_LOAD_PHASE_IDLE = 0,
    MAP_GNS_LOAD_PHASE_RESOURCES = 1, /* loading the pending resource files one by one */
    MAP_GNS_LOAD_PHASE_START = 15     /* choose the resources once the GNS is in */
} gns_load_phase_e;

typedef enum mesh_load_state { MAP_MESH_LOAD_IDLE = 0, MAP_MESH_LOAD_WAITING = 0x8d } mesh_load_state_e;

extern back_color_t g_map_back_color;
extern back_color_t g_map_saved_back_color;

/* GNS files and the loader. */
extern move_find_map_data_t g_move_find_map_data[MAP_ID_COUNT];
/* Zero where the map ID has no file. */
extern u32 g_map_gns_lba[MAP_ID_COUNT];
extern map_resource_t* g_map_mesh_load_buffer;
extern gns_load_phase_e g_map_gns_load_phase;
extern resource_load_state_e g_map_resource_load_state;
extern mesh_load_state_e g_map_mesh_load_state;
extern map_gns_record_t g_map_selected_mesh_record;
extern s32 g_map_loaded_id;
extern map_resource_t* g_map_data_load_buffer;
extern map_gns_record_t g_map_gns_records[150];
extern u32 g_map_gns_record_cursor;

/* GNS records selected for loading and the ones currently loaded, indexed by resource type. */
extern map_gns_record_t g_map_loaded_gns_records[GNS_RESOURCE_TABLE_COUNT];
extern u8 g_map_gns_dispatch_held;

/* Resources read but not yet applied: the palette and lighting data wait for MAP_RESOURCE_APPLY_PALETTES and
 * MAP_RESOURCE_APPLY_LIGHTING, and the texture flags mark the whole texture or the quarters read into the
 * load buffer, uploaded after the texture read completes. */
extern u8 g_map_palette_pending;
extern u8 g_map_pending_palette_data[0x200];
extern u8 g_map_lighting_pending;
extern u8 g_map_pending_lighting_data[0x2d];
extern u8 g_map_texture_pending;
extern u8 g_map_texture_quarter_0_pending;
extern u8 g_map_texture_quarter_1_pending;
extern u8 g_map_texture_quarter_2_pending;

/* The polygon tables of every mesh part, appended part by part as the meshes load. */
extern map_mesh_triangle_positions_t g_map_textured_triangle_positions[RENDER_TEXTURED_TRIANGLE_COUNT];
extern map_mesh_quad_positions_t g_map_textured_quad_positions[RENDER_TEXTURED_QUAD_COUNT];
extern map_mesh_triangle_positions_t g_map_untextured_triangle_positions[RENDER_UNTEXTURED_TRIANGLE_COUNT];
extern map_mesh_quad_positions_t g_map_untextured_quad_positions[RENDER_UNTEXTURED_QUAD_COUNT];
extern map_mesh_triangle_normals_t g_map_textured_triangle_normals[];
extern map_mesh_quad_normals_t g_map_textured_quad_normals[];
extern s32 g_map_textured_quad_count;
extern s32 g_map_textured_triangle_count;
extern s32 g_map_untextured_quad_count;
extern s32 g_map_untextured_triangle_count;

static u8 s_map_texture_quarter_3_pending = 0;
static s32 s_map_texture_animation_cycle_handles[MAP_TEXTURE_ANIMATION_COUNT] = { 0 };
static u8 s_saved_texture_modes[72] = { 0 };

static MATRIX s_map_camera_matrix = { 0 };

static DR_MOVE s_texture_animation_moves[MAP_TEXTURE_ANIMATION_COUNT] = { 0 };

/* Provisional: these two GNS work arrays are cleared but have no identified reader. */
static map_gns_load_info_t s_unread_gns_load_infos[150] = { 0 };

static u8 s_unread_gns_record_flags[150] = { 0 };

static render_buffer_t* s_last_render_buffer = 0;

static s32 s_textured_triangle_count = 0;

static s32 s_textured_quad_count = 0;

static s32 s_untextured_triangle_count = 0;

static s32 s_untextured_quad_count = 0;
static const map_mesh_geometry_t* s_map_mesh_part_geometry[9] = { 0 };
static render_buffer_t* s_map_tile_mark_color_buffer = { 0 };
static s32 s_weather_lightning_mode = 0;
static lightning_state_t s_weather_lightning_state = { 0 };
static s32 s_weather_timed_transform_frame = 0;
static map_mesh_triangle_positions_t* s_map_part_untextured_triangle_positions = { 0 };
static map_mesh_quad_positions_t* s_map_part_untextured_quad_positions = { 0 };
static map_gns_record_t s_map_pending_gns_records[GNS_RESOURCE_TABLE_COUNT] = { 0 };
static MATRIX s_map_local_light_matrix = { 0 };
static map_mesh_keyframe_t s_map_mesh_animation_keyframes[129] = { 0 }; /* index 0 unused; parts 1-8 reach 0x80 */
static u8 s_weather_fall_speed = 0;

static void dispatch_polygon_flag_command(s32 command);
static void set_light_matrices(MATRIX* first, MATRIX* second);
static void init_weather_particles(weather_command_e command);
static void update_mesh_animations(void);
static void update_texture_animations(void);
static map_state_record_t* load_mesh_animation_record(map_state_record_t* record);
static void append_mesh_geometry(const map_mesh_geometry_t* geometry_data, map_mesh_part_t* metadata);
static void init_mesh_render_records(const map_mesh_geometry_t* geometry_data);
static s32 stop_weather_sfx(void);

/* lighting */

/* Apply a map_polygon_flag_command_e to the textured map polygons: reset or clear
 * their per-frame lit flag, or repaint the unlit ones in the ambient color in the
 * current render buffer. The repaint's pointer-plus form, not gt3[i] indexing,
 * is what the bytes require (as in map_draw). */
static void dispatch_polygon_flag_command(s32 command) {
    s32 i;

    switch (command) {
    case MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT:
        for (i = 0; i < g_map_textured_triangle_count; i++) {
            if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_render_current_buffer->gt3 + i)->r0 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g0 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b0 = s_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt3 + i)->r1 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g1 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b1 = s_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt3 + i)->r2 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt3 + i)->g2 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt3 + i)->b2 = s_map_ambient_polygon_color.b;
            }
        }
        for (i = 0; i < g_map_textured_quad_count; i++) {
            if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                (g_render_current_buffer->gt4 + i)->r0 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g0 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b0 = s_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r1 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g1 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b1 = s_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r2 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g2 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b2 = s_map_ambient_polygon_color.b;
                (g_render_current_buffer->gt4 + i)->r3 = s_map_ambient_polygon_color.r;
                (g_render_current_buffer->gt4 + i)->g3 = s_map_ambient_polygon_color.g;
                (g_render_current_buffer->gt4 + i)->b3 = s_map_ambient_polygon_color.b;
            }
        }
        g_map_ambient_restored_buffer = g_render_current_buffer;
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

/*
 * Copy two matrices into the working pair at g_map_light_color_matrix and into entries 0 and
 * 2 of s_weather_lightning_state.color_matrices, then load the first into the GTE color matrix.
 *
 * Each backup copy rereads the working field; a chained assignment assigns the
 * loop pointers to different registers.
 */
static void set_light_matrices(MATRIX* first, MATRIX* second) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_map_light_color_matrix.m[i][j] = first->m[i][j];
            s_weather_lightning_state.color_matrices[0].m[i][j] = g_map_light_color_matrix.m[i][j];
        }
        g_map_light_color_matrix.t[i] = first->t[i];
        s_weather_lightning_state.color_matrices[0].t[i] = g_map_light_color_matrix.t[i];
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            g_map_light_matrix.m[i][j] = second->m[i][j];
            s_weather_lightning_state.color_matrices[2].m[i][j] = g_map_light_matrix.m[i][j];
        }
        g_map_light_matrix.t[i] = second->t[i];
        s_weather_lightning_state.color_matrices[2].t[i] = g_map_light_matrix.t[i];
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
s32* map_light_dispatch_back_color_command(s32 command, s32 value_1, s32 value_2, s32 value_3) {
    /* Left unset on the DISABLE, RESTORE and default paths. */
    s32* result;

    switch (command) {
    case MAP_LIGHT_BACK_COLOR_SET: {
        s32* stored_values;

        if (g_map_back_color_disabled == 1) {
            return 0;
        }
        stored_values = &g_map_back_color.red;
        *stored_values = value_1;
        g_map_back_color.green = value_2;
        g_map_back_color.blue = value_3;
        SetBackColor(value_1, value_2, value_3);
        return stored_values;
    }
    case MAP_LIGHT_BACK_COLOR_GET:
        result = &g_map_back_color.red;
        break;
    case MAP_LIGHT_BACK_COLOR_DISABLE:
        SetBackColor(0, 0, 0);
        g_map_back_color_disabled = 1;
        return result;
    case MAP_LIGHT_BACK_COLOR_RESTORE:
        SetBackColor(g_map_back_color.red, g_map_back_color.green, g_map_back_color.blue);
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
 * map_light_dispatch_command.
 */
MATRIX* map_light_dispatch_matrix_command(
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
void* map_light_dispatch_command(map_light_command_e command, void* data) {
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
        g_map_back_color.red = ((CVECTOR*)data)->r;
        g_map_back_color.green = ((CVECTOR*)data)->g;
        g_map_back_color.blue = ((CVECTOR*)data)->b;
        SetBackColor(g_map_back_color.red, g_map_back_color.green, g_map_back_color.blue);
        break;
    case MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR:
        g_map_saved_back_color.red = g_map_back_color.red;
        g_map_saved_back_color.green = g_map_back_color.green;
        g_map_saved_back_color.blue = g_map_back_color.blue;
        break;
    case MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR:
        g_map_back_color.red = g_map_saved_back_color.red;
        g_map_back_color.green = g_map_saved_back_color.green;
        g_map_back_color.blue = g_map_saved_back_color.blue;
        break;
    case MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR: {
        CVECTOR* color = &g_map_cached_back_color;

        color->r = g_map_back_color.red;
        g_map_cached_back_color.g = g_map_back_color.green;
        g_map_cached_back_color.b = g_map_back_color.blue;
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

/* drawing */

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
void map_draw(MATRIX* camera) {
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
    map_tile_t* ground_tile;

    /* The backdrop gradient and its draw modes go in ordering-table slot 0x17F, the first drawn. */
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay_modes[1]);
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay);
    AddPrim(g_render_current_buffer->otag + 0x17F, &g_render_current_buffer->overlay_modes[0]);

    update_texture_animations();
    update_mesh_animations();

    /* Lightning strikes at random, about once every 480 frames, with one of two thunder sounds. */
    if (s_weather_lightning_mode != MAP_LIGHTNING_OFF && g_weather_sfx_enabled) {
        if (rand() % 480 == 0) {
            s_weather_lightning_state.timer = 60;
            if (rand() & 1) {
                g_weather_thunder_a_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_A);
                sound_play_sfx_on_free_channel(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_A));
            } else {
                g_weather_thunder_b_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_B);
                sound_play_sfx_on_free_channel(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_THUNDER_B));
            }
        }

        /* Over the 60-frame flash the gradient eases from the flash colors back to its base colors. */
        if (s_weather_lightning_state.timer != 0) {
            u32 remaining;

            remaining = --s_weather_lightning_state.timer;
            s_weather_lightning_state.colors[0].r = s_weather_lightning_state.base_colors[0].r
                - s_weather_lightning_state.base_colors[0].r * remaining / 60 + remaining * 208 / 60;
            s_weather_lightning_state.colors[0].g = s_weather_lightning_state.base_colors[0].g
                - s_weather_lightning_state.base_colors[0].g * remaining / 60 + remaining * 208 / 60;
            s_weather_lightning_state.colors[0].b = s_weather_lightning_state.base_colors[0].b
                - s_weather_lightning_state.base_colors[0].b * remaining / 60 + remaining * 3;
            s_weather_lightning_state.colors[1].r = s_weather_lightning_state.base_colors[1].r
                - s_weather_lightning_state.base_colors[1].r * remaining / 60 + remaining * 112 / 60;
            s_weather_lightning_state.colors[1].g = s_weather_lightning_state.base_colors[1].g
                - s_weather_lightning_state.base_colors[1].g * remaining / 60 + remaining * 112 / 60;
            s_weather_lightning_state.colors[1].b = s_weather_lightning_state.base_colors[1].b
                - s_weather_lightning_state.base_colors[1].b * remaining / 60 + remaining * 96 / 60;
            map_light_dispatch_command(MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT, &s_weather_lightning_state.colors[0]);
        } else {
            map_light_dispatch_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
            if (s_weather_lightning_mode == MAP_LIGHTNING_ONCE) {
                s_weather_lightning_mode = MAP_LIGHTNING_OFF;
            }
        }

        /* Flicker the map lighting: color matrix 1 with a blue back color on frames 58 and 56,
         * the normal matrix and back color on frames 57 and 51. */
        if (s_weather_lightning_state.timer == 0x3A || s_weather_lightning_state.timer == 0x38) {
            SetColorMatrix(&s_weather_lightning_state.color_matrices[1]);
            SetBackColor(0, 0, 0x40);
        }
        if (s_weather_lightning_state.timer == 0x39 || s_weather_lightning_state.timer == 0x33) {
            SetColorMatrix(&s_weather_lightning_state.color_matrices[0]);
            SetBackColor(g_map_back_color.red, g_map_back_color.green, g_map_back_color.blue);
        }
    }

    if (g_map_light_transition_command) {
        map_light_dispatch_matrix_command(
            g_map_light_transition_command, &g_map_light_color_matrix, &g_map_light_matrix, 0);
    }

    /* Color the backdrop polygon: the top gradient color on its upper vertices, the bottom on the lower. */
    g_render_current_buffer->overlay.r0 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_render_current_buffer->overlay.g0 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_render_current_buffer->overlay.b0 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_render_current_buffer->overlay.r1 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r;
    g_render_current_buffer->overlay.g1 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g;
    g_render_current_buffer->overlay.b1 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b;
    g_render_current_buffer->overlay.r2 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_render_current_buffer->overlay.g2 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_render_current_buffer->overlay.b2 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;
    g_render_current_buffer->overlay.r3 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r;
    g_render_current_buffer->overlay.g3 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g;
    g_render_current_buffer->overlay.b3 = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b;

    MulMatrix0(&g_map_light_matrix, &g_map_light_rotation_matrix, &s_map_local_light_matrix);
    SetLightMatrix(&s_map_local_light_matrix);

    if (g_map_polygon_flag_clear_countdown != 0) {
        if (--g_map_polygon_flag_clear_countdown == 0) {
            dispatch_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE);
        }
    }

    /* Part 0, the static map, is drawn with the camera transform alone. */
    SetRotMatrix(camera);
    SetTransMatrix(camera);
    g_map_part_textured_triangle_count = g_map_mesh_parts[0].textured_triangle_count;
    g_map_part_textured_triangle_positions
        = &g_map_textured_triangle_positions[g_map_mesh_parts[0].textured_triangle_start];
    g_map_part_textured_triangle_normals
        = &g_map_textured_triangle_normals[g_map_mesh_parts[0].textured_triangle_start];
    map_queue_textured_triangles(g_render_current_buffer->otag,
        &g_render_current_buffer->gt3[g_map_mesh_parts[0].textured_triangle_start], g_map_camera_rotation.vy, &i);

    g_map_part_textured_quad_count = g_map_mesh_parts[0].textured_quad_count;
    g_map_part_textured_quad_positions = &g_map_textured_quad_positions[g_map_mesh_parts[0].textured_quad_start];
    g_map_part_textured_quad_normals = &g_map_textured_quad_normals[g_map_mesh_parts[0].textured_quad_start];
    map_queue_textured_quads(g_render_current_buffer->otag,
        &g_render_current_buffer->gt4[g_map_mesh_parts[0].textured_quad_start], g_map_camera_rotation.vy, &i);

    g_map_part_untextured_triangle_count = g_map_mesh_parts[0].untextured_triangle_count;
    s_map_part_untextured_triangle_positions
        = &g_map_untextured_triangle_positions[g_map_mesh_parts[0].untextured_triangle_start];
    map_queue_untextured_triangles(g_render_current_buffer->otag,
        &g_render_current_buffer->f3[g_map_mesh_parts[0].untextured_triangle_start], g_map_camera_rotation.vy, &i);

    g_map_part_untextured_quad_count = g_map_mesh_parts[0].untextured_quad_count;
    s_map_part_untextured_quad_positions = &g_map_untextured_quad_positions[g_map_mesh_parts[0].untextured_quad_start];
    map_queue_untextured_quads(g_render_current_buffer->otag,
        &g_render_current_buffer->f4[g_map_mesh_parts[0].untextured_quad_start], g_map_camera_rotation.vy, &i);

    /* Dead store: `matrix` is reassigned for every drawn child part before
     * use. It still survives the first CSE pass (the loop uses `matrix`), and
     * the second pass reuses it for the loop-hoisted &g_map_mesh_parts[0].matrix
     * constant; reload then deletes that spilled constant together with this
     * set. The transient local moves the draw calls' `&i` temporary from v0
     * to a0, as in the target. Earlier than the textured_triangle_count access, it would
     * become the base of that address instead. */
    matrix = &g_map_mesh_parts[0].matrix;
    /* Parts 1-8 that have an animation are posed relative to their parent part
     * and lit by their own rotation. */
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
            MulMatrix0(&s_map_local_light_matrix, &light_rotation, &light_matrix);
            SetLightMatrix(&light_matrix);
            SetRotMatrix(matrix);
            SetTransMatrix(matrix);

            g_map_part_textured_triangle_count = g_map_mesh_parts[part].textured_triangle_count;
            g_map_part_textured_triangle_positions
                = &g_map_textured_triangle_positions[g_map_mesh_parts[part].textured_triangle_start];
            g_map_part_textured_triangle_normals
                = &g_map_textured_triangle_normals[g_map_mesh_parts[part].textured_triangle_start];
            map_queue_textured_triangles(g_render_current_buffer->otag,
                &g_render_current_buffer->gt3[g_map_mesh_parts[part].textured_triangle_start], g_map_camera_rotation.vy,
                &i);
            g_map_part_textured_quad_count = g_map_mesh_parts[part].textured_quad_count;
            g_map_part_textured_quad_positions
                = &g_map_textured_quad_positions[g_map_mesh_parts[part].textured_quad_start];
            g_map_part_textured_quad_normals = &g_map_textured_quad_normals[g_map_mesh_parts[part].textured_quad_start];
            map_queue_textured_quads(g_render_current_buffer->otag,
                &g_render_current_buffer->gt4[g_map_mesh_parts[part].textured_quad_start], g_map_camera_rotation.vy,
                &i);
            g_map_part_untextured_triangle_count = g_map_mesh_parts[part].untextured_triangle_count;
            s_map_part_untextured_triangle_positions
                = &g_map_untextured_triangle_positions[g_map_mesh_parts[part].untextured_triangle_start];
            map_queue_untextured_triangles(g_render_current_buffer->otag,
                &g_render_current_buffer->f3[g_map_mesh_parts[part].untextured_triangle_start],
                g_map_camera_rotation.vy, &i);
            g_map_part_untextured_quad_count = g_map_mesh_parts[part].untextured_quad_count;
            s_map_part_untextured_quad_positions
                = &g_map_untextured_quad_positions[g_map_mesh_parts[part].untextured_quad_start];
            map_queue_untextured_quads(g_render_current_buffer->otag,
                &g_render_current_buffer->f4[g_map_mesh_parts[part].untextured_quad_start], g_map_camera_rotation.vy,
                &i);

            /* While an ability effect runs on these three maps, keyframes 0x61 onward give four points per part;
             * transform them by the part's pose into g_effect_map_vertices. */
            if (g_map_untextured_otz_mask && g_effect_map_vertices
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
                    RotTrans(&s_map_mesh_animation_keyframes[j + effect_base].translation,
                        g_effect_map_vertices + j + (part - 1) * 4, &effect_flag);
                }
            }
        }
    }
    PopMatrix();

    /* Snow: 64 flakes in a slowly spinning volume; a flake that falls below the screen respawns at the top. */
    if (g_weather_effect_mode == WEATHER_EFFECT_SNOW) {
        PushMatrix();
        g_weather_rotation.vy += g_weather_primary_speed;
        RotMatrix(&g_weather_rotation, &g_weather_matrix);
        TransMatrix(&g_weather_matrix, &g_weather_translation);
        ScaleMatrix(&g_weather_matrix, &g_weather_scale);
        SetRotMatrix(&g_weather_matrix);
        SetTransMatrix(&g_weather_matrix);

        half_size = g_map_camera_zoom.vx * g_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            /* The ground height below the flake is computed but never used in this mode. */
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_weather_particles[i].x / MAP_TILE_SIZE;
            ground_tile = &g_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * MAP_HEIGHT_STEP);

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
                g_weather_particles[i].fall_speed = sprite + s_weather_fall_speed;
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
            if (!(rand() & 0x1F)) {
                if (rand() & 1) {
                    g_weather_particles[i].x++;
                } else {
                    g_weather_particles[i].x--;
                }
            }
            if (!(rand() & 0x1F)) {
                if (rand() & 1) {
                    g_weather_particles[i].z++;
                } else {
                    g_weather_particles[i].z--;
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
    if (g_weather_effect_mode == WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
        s32 sine;

        PushMatrix();
        g_weather_timed_transform_rotation.vy = g_map_camera_rotation.vy;
        g_weather_timed_transform_rotation.vz
            = -(rsin((s_weather_timed_transform_frame << 10) / g_weather_timed_transform_duration) >> 4);
        sine = rsin((s_weather_timed_transform_frame << 10) / g_weather_timed_transform_duration);
        g_weather_scale.vx = sine * 4 + ONE;
        g_weather_scale.vz = sine * 4 + ONE;
        g_weather_scale.vy = sine + ONE;
        s_weather_timed_transform_frame++;
        RotMatrix(&g_weather_timed_transform_rotation, &g_weather_matrix);
        TransMatrix(&g_weather_matrix, &g_weather_translation);
        ScaleMatrix(&g_weather_matrix, &g_weather_scale);
        SetRotMatrix(&g_weather_matrix);
        SetTransMatrix(&g_weather_matrix);

        half_size = g_map_camera_zoom.vx * g_weather_sprite_scale / 0x100000;
        for (i = 0; i < 64; i++) {
            /* Both ground heights are computed and discarded, as in snow. */
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_weather_particles[i].x / MAP_TILE_SIZE;
            ground_tile = &g_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * MAP_HEIGHT_STEP);
            tile = (g_weather_particles[i + 32].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_weather_particles[i + 32].x / MAP_TILE_SIZE;
            ground_tile = &g_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * MAP_HEIGHT_STEP);

            /* Respawn particles that leave the screen: at the top while falling, at the bottom while rising. */
            if ((s_weather_timed_transform_frame < g_weather_timed_transform_duration / 8
                    && g_render_buffers[0].ft3[i].y0 >= 0x119)
                || (g_weather_timed_transform_duration / 8 < s_weather_timed_transform_frame
                    && g_render_buffers[0].ft3[i].y0 < 0)) {
                if (g_weather_scale.vy != 0) {
                    if (s_weather_timed_transform_frame < g_weather_timed_transform_duration / 8) {
                        g_weather_particles[i].y = -0x100000 / g_weather_scale.vy;
                    }
                    if (g_weather_timed_transform_duration / 8 < s_weather_timed_transform_frame) {
                        g_weather_particles[i].y = 0x30000 / g_weather_scale.vy;
                    }
                }
                sprite = rand() % 3;
                g_weather_particles[i].fall_speed = s_weather_fall_speed + 3 - sprite;

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

            if (!(rand() & 0x1F)) {
                if (rand() & 1) {
                    g_weather_particles[i].x++;
                } else {
                    g_weather_particles[i].x--;
                }
            }
            if (!(rand() & 0x1F)) {
                if (rand() & 1) {
                    g_weather_particles[i].z++;
                } else {
                    g_weather_particles[i].z--;
                }
            }

            if (s_weather_timed_transform_frame < g_weather_timed_transform_duration / 8) {
                g_weather_particles[i].y += g_weather_particles[i].fall_speed * g_system_vsync_interval
                    * rsin((s_weather_timed_transform_frame << 13) / g_weather_timed_transform_duration + 0x400) / 2048;
            }
            if (g_weather_timed_transform_duration / 8 < s_weather_timed_transform_frame) {
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
        if (s_weather_timed_transform_frame == g_weather_timed_transform_duration) {
            weather_dispatch_command(WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
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
    if (g_weather_effect_mode == WEATHER_EFFECT_RAIN) {
        half_size = g_map_camera_zoom.vx * 22;
        half_size /= 0x2000;
        for (i = 0; i < 32; i++) {
            g_map_tile_data_ptr = g_map_tile_data;
            tile = (g_weather_particles[i].z / MAP_TILE_SIZE) * g_map_tile_width
                + g_weather_particles[i].x / MAP_TILE_SIZE;
            ground_tile = &g_map_tile_data_ptr[tile];
            ground = -((ground_tile->height + ground_tile->depth_half_height.depth) * MAP_HEIGHT_STEP);

            /* A streak whose upper end passed the ground respawns at a random spot high above the map. */
            if (ground < g_weather_particles[i + 32].y) {
                g_weather_particles[i].fall_speed = WEATHER_RAIN_FALLING;
                g_weather_particles[i].x = rand() % (g_map_tile_width * MAP_TILE_SIZE);
                g_weather_particles[i].z = rand() % (g_map_tile_depth * MAP_TILE_SIZE);
                spawn_y = -0x180;
                g_weather_particles[i].y = spawn_y;
                g_weather_particles[i + 32].x = g_weather_particles[i].x;
                g_weather_particles[i + 32].z = g_weather_particles[i].z;
                g_weather_particles[i + 32].y
                    = spawn_y - s_weather_fall_speed - rand() % (g_weather_primary_speed * 12);
            }

            g_weather_particles[i].y += s_weather_fall_speed * g_system_vsync_interval;
            g_weather_particles[i + 32].y += s_weather_fall_speed * g_system_vsync_interval;
            if (i < 16) {
                g_weather_particles[i].y += g_weather_primary_speed * g_system_vsync_interval;
                g_weather_particles[i + 32].y += g_weather_primary_speed * g_system_vsync_interval;
            } else {
                g_weather_particles[i].y += g_weather_rain_secondary_fall_speed * g_system_vsync_interval;
                g_weather_particles[i + 32].y += g_weather_rain_secondary_fall_speed * g_system_vsync_interval;
            }

            /* A lower end past the ground is parked at g_map_lowest_surface_y, which the splash test
             * below recognizes; a streak still above the ground is drawn as a thin triangle. */
            if (g_weather_particles[i].y >= -0x17F) {
                if (ground < g_weather_particles[i].y) {
                    g_weather_particles[i].y = g_map_lowest_surface_y;
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
                    || g_weather_particles[i].y == g_map_lowest_surface_y)) {
                g_weather_rain_splash_particles[i].fall_speed = 0x10;
                g_weather_rain_splash_particles[i].y = ground - 8;
                g_weather_rain_splash_particles[i].x = g_weather_particles[i].x;
                g_weather_rain_splash_particles[i].z = g_weather_particles[i].z;
                if (!(g_map_tile_data_ptr[tile].flags.untargetable)) {
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
                if (!(frame & 3)) {
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
     * rather than g_render_current_buffer->gt3[i]/g_render_current_buffer->gt4[i] indexing: that keeps the target's
     * `addu offset, base` operand order (as in target_set_tile_background_color). */
    if (g_map_ambient_restored_buffer) {
        /* Ambient colors repainted in one render buffer are repeated once in the other. */
        if (g_map_ambient_restored_buffer != g_render_current_buffer) {
            for (i = 0; i < g_map_textured_triangle_count; i++) {
                if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_render_current_buffer->gt3 + i)->r0 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g0 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b0 = s_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt3 + i)->r1 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g1 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b1 = s_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt3 + i)->r2 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt3 + i)->g2 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt3 + i)->b2 = s_map_ambient_polygon_color.b;
                }
            }
            for (i = 0; i < g_map_textured_quad_count; i++) {
                if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                    (g_render_current_buffer->gt4 + i)->r0 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g0 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b0 = s_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r1 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g1 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b1 = s_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r2 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g2 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b2 = s_map_ambient_polygon_color.b;
                    (g_render_current_buffer->gt4 + i)->r3 = s_map_ambient_polygon_color.r;
                    (g_render_current_buffer->gt4 + i)->g3 = s_map_ambient_polygon_color.g;
                    (g_render_current_buffer->gt4 + i)->b3 = s_map_ambient_polygon_color.b;
                }
            }
        }
        g_map_ambient_restored_buffer = 0;
    }

    /* map_set_tile_mark_colors recolors one render buffer; a SHOW it leaves is
     * repeated here once on this frame's buffer, the other of the pair, then marked shown. */
    g_map_tile_data_ptr = g_map_tile_data;
    if (g_map_tile_mark_modes.move == MAP_TILE_MARK_SHOW_MOVE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                /* terrain_tile holds z in bits 1-7, x in bits 8-15 and the upper level in bit 0 (tiles 0x100 on). */
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    /* Keep the CLUT's x and switch it to the marked-tiles palette row. */
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x20;
                    g_render_current_buffer->gt3[i].r1 = 0x20;
                    g_render_current_buffer->gt3[i].r2 = 0x20;
                    g_render_current_buffer->gt3[i].g0 = 0x28;
                    g_render_current_buffer->gt3[i].g1 = 0x28;
                    g_render_current_buffer->gt3[i].g2 = 0x28;
                    g_render_current_buffer->gt3[i].b0 = 0x60;
                    g_render_current_buffer->gt3[i].b1 = 0x60;
                    g_render_current_buffer->gt3[i].b2 = 0x60;
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                }
            }
        }

        g_map_tile_mark_modes.move = MAP_TILE_MARK_MOVE_SHOWN;
    }

    if (g_map_tile_mark_modes.range == MAP_TILE_MARK_SHOW_RANGE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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

        g_map_tile_mark_modes.range = MAP_TILE_MARK_RANGE_SHOWN;
    }

    if (g_map_tile_mark_modes.targets == MAP_TILE_MARK_SHOW_TARGETS) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x50;
                    g_render_current_buffer->gt3[i].r1 = 0x50;
                    g_render_current_buffer->gt3[i].r2 = 0x50;
                    g_render_current_buffer->gt3[i].g0 = 0x60;
                    g_render_current_buffer->gt3[i].g1 = 0x60;
                    g_render_current_buffer->gt3[i].g2 = 0x60;
                    g_render_current_buffer->gt3[i].b0 = 0x10;
                    g_render_current_buffer->gt3[i].b1 = 0x10;
                    g_render_current_buffer->gt3[i].b2 = 0x10;
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7F;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803F) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                }
            }
        }

        g_map_tile_mark_modes.targets = MAP_TILE_MARK_TARGETS_SHOWN;
    }

    /* A HIDE copies the CLUTs, and some colors, back from the buffer
     * map_set_tile_mark_colors restored. */
    if (g_map_tile_mark_modes.move == MAP_TILE_MARK_HIDE_MOVE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            g_render_current_buffer->gt3[i].clut = s_map_tile_mark_color_buffer->gt3[i].clut;
            if (!(g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                g_render_current_buffer->gt3[i].r0 = s_map_tile_mark_color_buffer->gt3[i].r0;
                g_render_current_buffer->gt3[i].r1 = s_map_tile_mark_color_buffer->gt3[i].r1;
                g_render_current_buffer->gt3[i].r2 = s_map_tile_mark_color_buffer->gt3[i].r2;
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            g_render_current_buffer->gt4[i].clut = s_map_tile_mark_color_buffer->gt4[i].clut;
            if (!(g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT)) {
                g_render_current_buffer->gt4[i].r0 = s_map_tile_mark_color_buffer->gt4[i].r0;
                g_render_current_buffer->gt4[i].r1 = s_map_tile_mark_color_buffer->gt4[i].r1;
                g_render_current_buffer->gt4[i].r2 = s_map_tile_mark_color_buffer->gt4[i].r2;
                g_render_current_buffer->gt4[i].r3 = s_map_tile_mark_color_buffer->gt4[i].r3;
            }
        }

        g_map_tile_mark_modes.move = MAP_TILE_MARK_NONE;
        /* With no layer left, stop the marked-tiles palette pulse and restore its colors. */
        if (!(g_map_tile_mark_modes.range | g_map_tile_mark_modes.targets)) {
            map_palette_modify(MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            map_palette_modify(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }

    if (g_map_tile_mark_modes.range == MAP_TILE_MARK_HIDE_RANGE) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            g_render_current_buffer->gt3[i].clut = s_map_tile_mark_color_buffer->gt3[i].clut;
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            g_render_current_buffer->gt4[i].clut = s_map_tile_mark_color_buffer->gt4[i].clut;
        }

        g_map_tile_mark_modes.range = MAP_TILE_MARK_NONE;
        if (!(g_map_tile_mark_modes.move | g_map_tile_mark_modes.targets)) {
            map_palette_modify(MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            map_palette_modify(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }

    if (g_map_tile_mark_modes.targets == MAP_TILE_MARK_HIDE_TARGETS) {
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            g_render_current_buffer->gt3[i].clut = s_map_tile_mark_color_buffer->gt3[i].clut;
            g_render_current_buffer->gt3[i].r0 = s_map_tile_mark_color_buffer->gt3[i].r0;
            g_render_current_buffer->gt3[i].r1 = s_map_tile_mark_color_buffer->gt3[i].r1;
            g_render_current_buffer->gt3[i].r2 = s_map_tile_mark_color_buffer->gt3[i].r2;
            g_render_current_buffer->gt3[i].g0 = s_map_tile_mark_color_buffer->gt3[i].g0;
            g_render_current_buffer->gt3[i].g1 = s_map_tile_mark_color_buffer->gt3[i].g1;
            g_render_current_buffer->gt3[i].g2 = s_map_tile_mark_color_buffer->gt3[i].g2;
            g_render_current_buffer->gt3[i].b0 = s_map_tile_mark_color_buffer->gt3[i].b0;
            g_render_current_buffer->gt3[i].b1 = s_map_tile_mark_color_buffer->gt3[i].b1;
            g_render_current_buffer->gt3[i].b2 = s_map_tile_mark_color_buffer->gt3[i].b2;
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            g_render_current_buffer->gt4[i].clut = s_map_tile_mark_color_buffer->gt4[i].clut;
            g_render_current_buffer->gt4[i].r0 = s_map_tile_mark_color_buffer->gt4[i].r0;
            g_render_current_buffer->gt4[i].r1 = s_map_tile_mark_color_buffer->gt4[i].r1;
            g_render_current_buffer->gt4[i].r2 = s_map_tile_mark_color_buffer->gt4[i].r2;
            g_render_current_buffer->gt4[i].r3 = s_map_tile_mark_color_buffer->gt4[i].r3;
            g_render_current_buffer->gt4[i].g0 = s_map_tile_mark_color_buffer->gt4[i].g0;
            g_render_current_buffer->gt4[i].g1 = s_map_tile_mark_color_buffer->gt4[i].g1;
            g_render_current_buffer->gt4[i].g2 = s_map_tile_mark_color_buffer->gt4[i].g2;
            g_render_current_buffer->gt4[i].g3 = s_map_tile_mark_color_buffer->gt4[i].g3;
            g_render_current_buffer->gt4[i].b0 = s_map_tile_mark_color_buffer->gt4[i].b0;
            g_render_current_buffer->gt4[i].b1 = s_map_tile_mark_color_buffer->gt4[i].b1;
            g_render_current_buffer->gt4[i].b2 = s_map_tile_mark_color_buffer->gt4[i].b2;
            g_render_current_buffer->gt4[i].b3 = s_map_tile_mark_color_buffer->gt4[i].b3;
        }

        g_map_tile_mark_modes.targets = MAP_TILE_MARK_NONE;
        if (!(g_map_tile_mark_modes.move | g_map_tile_mark_modes.range)) {
            map_palette_modify(MAP_COLOR_MODE_STOP_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
            map_palette_modify(MAP_COLOR_MODE_RESTORE, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, -0x20, -0x20, -0x20);
        }
    }
}

/* Recolor the current render buffer's map polygons for one tile-mark layer
 * (map_tile_mark_mode_e): SHOW tints marked tiles blue (move), red (range) or yellow (targets),
 * HIDE restores them (a hidden target tile in range turns red again) and NONE restores every
 * polygon. The first layer shown starts the marked-tiles palette pulse; the second argument is
 * unused. */
void map_set_tile_mark_colors(s32 mode, s32 unused) {
    s32 i;
    s16 other_state_a;
    s16 other_state_b;
    s16 tile;
    map_tile_marks_t marks;

    g_map_tile_data_ptr = g_map_tile_data;
    switch (mode) {
    case MAP_TILE_MARK_NONE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            g_render_current_buffer->gt3[i].clut
                = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }
        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            g_render_current_buffer->gt4[i].clut
                = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        }

        /* Leave every layer in HIDE so the next draw copies this buffer's colors into the other one. */
        g_map_tile_mark_modes.move = MAP_TILE_MARK_HIDE_MOVE;
        g_map_tile_mark_modes.range = MAP_TILE_MARK_HIDE_RANGE;
        g_map_tile_mark_modes.targets = MAP_TILE_MARK_HIDE_TARGETS;
        dispatch_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        dispatch_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT);
        s_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case MAP_TILE_MARK_SHOW_MOVE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_map_tile_mark_modes.move = MAP_TILE_MARK_SHOW_MOVE;
        /* Start the marked-tiles palette pulse unless another layer is already shown. */
        other_state_a = g_map_tile_mark_modes.range;
        other_state_b = g_map_tile_mark_modes.targets;
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == MAP_TILE_MARK_HIDE_RANGE && other_state_b == MAP_TILE_MARK_HIDE_TARGETS)) {
            map_palette_modify(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case MAP_TILE_MARK_HIDE_MOVE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    /* A lit polygon is relit from its normals again; an unlit one returns to the ambient color. */
                    if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt3 + i)->r0 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g0 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b0 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r1 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g1 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b1 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r2 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g2 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b2 = s_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.move_destination) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt4 + i)->r0 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g0 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b0 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r1 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g1 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b1 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r2 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g2 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b2 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r3 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g3 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b3 = s_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        g_map_tile_mark_modes.move = MAP_TILE_MARK_HIDE_MOVE;
        s_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case MAP_TILE_MARK_SHOW_RANGE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x60;
                    g_render_current_buffer->gt3[i].r1 = 0x60;
                    g_render_current_buffer->gt3[i].r2 = 0x60;
                    g_render_current_buffer->gt3[i].g0 = 0x18;
                    g_render_current_buffer->gt3[i].g1 = 0x18;
                    g_render_current_buffer->gt3[i].g2 = 0x18;
                    g_render_current_buffer->gt3[i].b0 = 0x20;
                    g_render_current_buffer->gt3[i].b1 = 0x20;
                    g_render_current_buffer->gt3[i].b2 = 0x20;
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_map_tile_mark_modes.range = MAP_TILE_MARK_SHOW_RANGE;
        other_state_a = g_map_tile_mark_modes.move;
        other_state_b = g_map_tile_mark_modes.targets;
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == MAP_TILE_MARK_HIDE_MOVE && other_state_b == MAP_TILE_MARK_HIDE_TARGETS)) {
            map_palette_modify(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case MAP_TILE_MARK_HIDE_RANGE:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt3 + i)->r0 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g0 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b0 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r1 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g1 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b1 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt3 + i)->r2 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt3 + i)->g2 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt3 + i)->b2 = s_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.ability_range) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                    if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                        g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                    } else {
                        (g_render_current_buffer->gt4 + i)->r0 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g0 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b0 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r1 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g1 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b1 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r2 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g2 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b2 = s_map_ambient_polygon_color.b;
                        (g_render_current_buffer->gt4 + i)->r3 = s_map_ambient_polygon_color.r;
                        (g_render_current_buffer->gt4 + i)->g3 = s_map_ambient_polygon_color.g;
                        (g_render_current_buffer->gt4 + i)->b3 = s_map_ambient_polygon_color.b;
                    }
                }
            }
        }

        g_map_tile_mark_modes.range = MAP_TILE_MARK_HIDE_RANGE;
        s_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    case MAP_TILE_MARK_SHOW_TARGETS:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt3[i].clut
                        = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
                    g_render_current_buffer->gt3[i].r0 = 0x50;
                    g_render_current_buffer->gt3[i].r1 = 0x50;
                    g_render_current_buffer->gt3[i].r2 = 0x50;
                    g_render_current_buffer->gt3[i].g0 = 0x60;
                    g_render_current_buffer->gt3[i].g1 = 0x60;
                    g_render_current_buffer->gt3[i].g2 = 0x60;
                    g_render_current_buffer->gt3[i].b0 = 0x10;
                    g_render_current_buffer->gt3[i].b1 = 0x10;
                    g_render_current_buffer->gt3[i].b2 = 0x10;
                    g_map_textured_triangle_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                if (g_map_tile_data_ptr[tile].ceiling_depth_and_marks.targeted) {
                    g_render_current_buffer->gt4[i].clut
                        = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MARKED_TILES, 0);
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
                    g_map_textured_quad_positions[i].polygon_flags &= ~MAP_POLYGON_FLAG_LIT_ACTIVE;
                }
            }
        }

        g_map_tile_mark_modes.targets = MAP_TILE_MARK_SHOW_TARGETS;
        other_state_a = g_map_tile_mark_modes.move;
        other_state_b = g_map_tile_mark_modes.range;
        if ((other_state_a | other_state_b) == 0
            || (other_state_a == MAP_TILE_MARK_HIDE_MOVE && other_state_b == MAP_TILE_MARK_HIDE_RANGE)) {
            map_palette_modify(MAP_COLOR_MODE_BASE_REPEAT, 8, MAP_CLUT_ROW_MARKED_TILES, 0, 1, 10, 10, 10);
        }
        break;
    case MAP_TILE_MARK_HIDE_TARGETS:
        for (i = 0; i < g_map_mesh_parts[0].textured_triangle_count; i++) {
            if (g_map_textured_triangle_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_triangle_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_triangle_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_triangle_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    /* A target tile still in ability range goes back to the range tint instead. */
                    if (!(marks.ability_range)) {
                        g_render_current_buffer->gt3[i].clut
                            = (g_render_current_buffer->gt3[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_map_textured_triangle_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_map_textured_triangle_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_render_current_buffer->gt3 + i)->r0 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g0 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b0 = s_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt3 + i)->r1 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g1 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b1 = s_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt3 + i)->r2 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt3 + i)->g2 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt3 + i)->b2 = s_map_ambient_polygon_color.b;
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

        for (i = 0; i < g_map_mesh_parts[0].textured_quad_count; i++) {
            if (g_map_textured_quad_positions[i].terrain_tile != MAP_POLYGON_TILE_NONE) {
                g_map_tile_mark_y = (g_map_textured_quad_positions[i].terrain_tile >> 1) & 0x7f;
                g_map_tile_mark_x = g_map_textured_quad_positions[i].terrain_tile >> 8;
                tile = g_map_tile_mark_y * g_map_tile_width + g_map_tile_mark_x;
                if (g_map_textured_quad_positions[i].terrain_tile & 1) {
                    tile += 0x100;
                }
                marks = g_map_tile_data_ptr[tile].ceiling_depth_and_marks;
                if (marks.targeted) {
                    if (!(marks.ability_range)) {
                        g_render_current_buffer->gt4[i].clut
                            = (g_render_current_buffer->gt4[i].clut & 0x803f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
                        if (g_map_textured_quad_positions[i].polygon_flags & MAP_POLYGON_FLAG_LIT) {
                            g_map_textured_quad_positions[i].polygon_flags |= MAP_POLYGON_FLAG_LIT_ACTIVE;
                        } else {
                            (g_render_current_buffer->gt4 + i)->r0 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g0 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b0 = s_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r1 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g1 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b1 = s_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r2 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g2 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b2 = s_map_ambient_polygon_color.b;
                            (g_render_current_buffer->gt4 + i)->r3 = s_map_ambient_polygon_color.r;
                            (g_render_current_buffer->gt4 + i)->g3 = s_map_ambient_polygon_color.g;
                            (g_render_current_buffer->gt4 + i)->b3 = s_map_ambient_polygon_color.b;
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

        g_map_tile_mark_modes.targets = MAP_TILE_MARK_HIDE_TARGETS;
        s_map_tile_mark_color_buffer = g_render_current_buffer;
        break;
    }
}

/* render state */

static const SVECTOR s_zero_svector = { 0, 0, 0, 0 };

static const VECTOR s_unit_scale_vector = { ONE, ONE, ONE, 0 };

/* Initializes both battle render buffers: every primitive in each pool gets its
 * GPU command code, the full-screen Gouraud overlay gets its corners and two
 * semi-transparent draw modes, and the ordering table pointers are bound.
 *
 * The function-scope counters are intentionally reused by each pool loop:
 * their lifetime reproduces the target register allocation. */
void gfx_init_render_buffers(void) {
    POLY_F3 prototype;
    s32 primitive_index;
    s32 buffer_index;

    g_render_current_buffer = &g_render_buffers[0];
    g_render_buffers[0].otag = g_gfx_battle_otags[0];
    g_render_buffers[1].otag = g_gfx_battle_otags[1];
    for (buffer_index = 0; buffer_index < 2; buffer_index++) {
        for (primitive_index = 0; primitive_index < RENDER_WEATHER_SPRITE_COUNT; primitive_index++) {
            SetPolyFT3(&g_render_buffers[buffer_index].ft3[primitive_index]);
        }
    }
    SetPolyF3(&prototype);
    g_map_current_darkness_color.r = 0;
    g_map_current_darkness_color.g = 0;
    g_map_current_darkness_color.b = 1;
    g_map_current_darkness_color.cd = prototype.code;
    SetPolyG4(&g_render_buffers[0].overlay);
    SetPolyG4(&g_render_buffers[1].overlay);
    setXY4(&g_render_buffers[0].overlay, 0, 0, 384, 0, 0, 240, 384, 240);
    setXY4(&g_render_buffers[1].overlay, 0, 0, 384, 0, 0, 240, 384, 240);
    SetDrawMode(&g_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    for (primitive_index = 0; primitive_index < RENDER_TEXTURED_TRIANGLE_COUNT; primitive_index++) {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            SetPolyGT3(&g_render_buffers[buffer_index].gt3[primitive_index]);
        }
    }
    for (primitive_index = 0; primitive_index < RENDER_TEXTURED_QUAD_COUNT; primitive_index++) {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            SetPolyGT4(&g_render_buffers[buffer_index].gt4[primitive_index]);
        }
    }
    for (primitive_index = 0; primitive_index < RENDER_UNTEXTURED_TRIANGLE_COUNT; primitive_index++) {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            SetPolyF3(&g_render_buffers[buffer_index].f3[primitive_index]);
        }
    }
    for (primitive_index = 0; primitive_index < RENDER_UNTEXTURED_QUAD_COUNT; primitive_index++) {
        for (buffer_index = 0; buffer_index < 2; buffer_index++) {
            SetPolyF4(&g_render_buffers[buffer_index].f4[primitive_index]);
        }
    }
    g_weather_rain_sfx_id = 0;
    g_weather_sfx_enabled = 0;
    g_effect_map_vertices = 0;
}

/* Reset the map render state before a new GNS map loads: no polygons or tile links, every textured
 * polygon lit, ambient color 0x80 gray, weather and lightning off, texture and mesh part
 * animations stopped, GNS resource records cleared. */
static void reset_render_state(void) {
    SVECTOR zero_vector = s_zero_svector;
    VECTOR unit_scale = s_unit_scale_vector;
    s32 i;
    s16 no_tile;
    s32 j;
    s32* weather_flags;

    g_map_tile_width = 1;
    g_map_tile_depth = 1;
    g_map_tile_mark_x = 1;
    g_map_tile_mark_y = 1;
    g_map_untextured_otz_mask = 0;
    g_map_untextured_fixed_ot_index = 4;
    g_map_gns_dispatch_held = 0;
    for (i = 0; i < 150; i++) {
        for (j = 0; j < 14; j++) {
            s_unread_gns_load_infos[i].bytes[j] = 0;
        }
        s_unread_gns_record_flags[i] = 0;
    }
    for (i = 0; i < GNS_RESOURCE_TABLE_COUNT; i++) {
        bzero(g_map_loaded_gns_records[i].load_info.bytes, sizeof(map_gns_load_info_t));
        bzero(&s_map_pending_gns_records[i].resource_type, 14);
    }
    weather_dispatch_command(WEATHER_COMMAND_CLEAR);
    s_weather_lightning_mode = MAP_LIGHTNING_OFF;
    /* Clear WEATHER_FLAG_IGNORE_WEATHER and the palette modifier above the low flags. */
    weather_flags = &g_weather_flags;
    *weather_flags &= WEATHER_LOW_FLAGS_MASK & ~WEATHER_FLAG_IGNORE_WEATHER;
    SetDrawMode(&g_render_buffers[0].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[1], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
    SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
    s_map_ambient_polygon_color.r = 0x80;
    s_map_ambient_polygon_color.g = 0x80;
    s_map_ambient_polygon_color.b = 0x80;
    for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
        g_map_textured_triangle_positions[i].polygon_flags = MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE;
    }
    for (i = 0; i < RENDER_TEXTURED_QUAD_COUNT; i++) {
        g_map_textured_quad_positions[i].polygon_flags = MAP_POLYGON_FLAG_LIT | MAP_POLYGON_FLAG_LIT_ACTIVE;
    }
    for (i = 0; i < RENDER_UNTEXTURED_TRIANGLE_COUNT; i++) {
        g_map_untextured_triangle_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < RENDER_UNTEXTURED_QUAD_COUNT; i++) {
        g_map_untextured_quad_positions[i].polygon_flags = 0;
    }
    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        g_map_texture_animations[i].active = 0;
    }
    /* A signed sentinel local retains each array's original -2 constant load. */
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
        g_map_textured_triangle_positions[i].terrain_tile = no_tile;
    }
    no_tile = MAP_POLYGON_TILE_NONE;
    for (i = 0; i < RENDER_TEXTURED_QUAD_COUNT; i++) {
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
void camera_update_matrices(MATRIX* camera_matrix, SVECTOR* rotation, VECTOR* screen_offset, VECTOR* camera_zoom) {
    s32 result;

    g_map_camera_zoom = *camera_zoom;
    memcpy(&g_map_camera_rotation, rotation, sizeof(g_map_camera_rotation));

    RotMatrix(rotation, camera_matrix);
    TransMatrix(camera_matrix, screen_offset);
    ScaleMatrix(camera_matrix, &g_map_camera_zoom);
    RotMatrix(rotation, &s_map_camera_matrix);
    SetRotMatrix(camera_matrix);
    SetTransMatrix(camera_matrix);
    RotTrans(&g_camera_integer_coords, (VECTOR*)s_map_camera_matrix.t, &result);
    ScaleMatrix(&s_map_camera_matrix, &g_map_camera_zoom);

    *camera_matrix = s_map_camera_matrix;
    g_map_mesh_parts[0].matrix = s_map_camera_matrix;
    RotMatrix(&g_map_light_rotation, &g_map_light_rotation_matrix);
}

/* weather */

/* Run a weather display command.
 *
 * APPLY_SCRIPT_WEATHER reads SCRIPT_VAR_WEATHER and issues the clear
 * command for no weather, or the set-up command for rain, storm or strong
 * storm, taking the snow variant on snowy maps; each choice also sets the
 * weather sounds and restores the lighting. The weather local is an s8: compares on
 * the promoted value copy it into a scratch register for the 3 and 4 tests,
 * as the target does. */
void weather_dispatch_command(u8 command) {
    s8 weather;

    switch (command) {
    case WEATHER_COMMAND_RESTORE_LIGHTING:
        map_light_dispatch_command(MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT, 0);
        SetColorMatrix(s_weather_lightning_state.color_matrices);
        SetBackColor(g_map_back_color.red, g_map_back_color.green, g_map_back_color.blue);
        break;
    case WEATHER_COMMAND_APPLY_SCRIPT_WEATHER:
        weather = script_variable_get(SCRIPT_VAR_WEATHER);
        s_weather_lightning_mode = MAP_LIGHTNING_OFF;
        weather &= 7;
        if (weather < WEATHER_RAIN) {
            stop_weather_sfx();
            weather_dispatch_command(WEATHER_COMMAND_CLEAR);
            weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_RAIN) {
            if (g_weather_flags & WEATHER_FLAG_SNOW) {
                stop_weather_sfx();
                weather_dispatch_command(WEATHER_COMMAND_LIGHT_SNOW);
            } else {
                stop_weather_sfx();
                weather_dispatch_command(WEATHER_COMMAND_LIGHT_RAIN);
            }
            weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_STORM) {
            if (g_weather_flags & WEATHER_FLAG_SNOW) {
                stop_weather_sfx();
                weather_dispatch_command(WEATHER_COMMAND_SNOW);
            } else {
                g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
                if (g_weather_sfx_enabled) {
                    sound_play_background(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
                }
                weather_dispatch_command(WEATHER_COMMAND_RAIN);
            }
            weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
        }
        if (weather == WEATHER_STRONG_STORM) {
            if (g_weather_flags & WEATHER_FLAG_SNOW) {
                stop_weather_sfx();
                weather_dispatch_command(WEATHER_COMMAND_HEAVY_SNOW);
                weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
            } else {
                stop_weather_sfx();
                weather_dispatch_command(WEATHER_COMMAND_HEAVY_RAIN);
                weather_dispatch_command(WEATHER_COMMAND_LIGHTNING);
                g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
                if (g_weather_sfx_enabled) {
                    sound_play_background(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
                }
            }
        }
        break;
    /* Both lightning commands zero color matrix 1, so on the flash's flicker
     * frames only the blue back color lights the map. */
    case WEATHER_COMMAND_THUNDER:
        s_weather_lightning_mode = MAP_LIGHTNING_ONCE;
        s_weather_lightning_state.timer = 0x3c;
        g_weather_rain_sfx_id = SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN);
        if (g_weather_sfx_enabled) {
            sound_play_background(SUZUKI_SOUND_ID(SOUND_RESOURCE_ENV, SOUND_ENV_SFX_RAIN));
        }
        s_weather_lightning_state.color_matrices[1].m[0][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[0][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[0][2] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][2] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][2] = 0;
        weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case WEATHER_COMMAND_LIGHTNING:
        s_weather_lightning_mode = MAP_LIGHTNING_RANDOM;
        s_weather_lightning_state.timer = 0x3c;
        s_weather_lightning_state.color_matrices[1].m[0][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[0][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[0][2] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[1][2] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][0] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][1] = 0;
        s_weather_lightning_state.color_matrices[1].m[2][2] = 0;
        weather_dispatch_command(WEATHER_COMMAND_RESTORE_LIGHTING);
        break;
    case WEATHER_COMMAND_SNOW:
        s_weather_fall_speed = 1;
        g_weather_primary_speed = 4;
        init_weather_particles(WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case WEATHER_COMMAND_LIGHT_SNOW:
        s_weather_fall_speed = 1;
        g_weather_primary_speed = 1;
        init_weather_particles(WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case WEATHER_COMMAND_HEAVY_SNOW:
        s_weather_fall_speed = 3;
        g_weather_primary_speed = 0x10;
        init_weather_particles(WEATHER_COMMAND_INIT_SNOW_PARTICLES);
        break;
    case WEATHER_COMMAND_RAIN:
        s_weather_fall_speed = 5;
        g_weather_primary_speed = 6;
        g_weather_rain_secondary_fall_speed = 9;
        init_weather_particles(WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case WEATHER_COMMAND_LIGHT_RAIN:
        s_weather_fall_speed = 3;
        g_weather_primary_speed = 5;
        g_weather_rain_secondary_fall_speed = 8;
        init_weather_particles(WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case WEATHER_COMMAND_HEAVY_RAIN:
        s_weather_fall_speed = 5;
        g_weather_primary_speed = 9;
        g_weather_rain_secondary_fall_speed = 0x12;
        init_weather_particles(WEATHER_COMMAND_INIT_RAIN_PARTICLES);
        break;
    case WEATHER_COMMAND_CLEAR:
        g_weather_effect_mode = WEATHER_EFFECT_CLEAR;
        s_weather_lightning_mode = MAP_LIGHTNING_OFF;
        break;
    }
}

/* Set up the weather particles and their flat-textured sprites.
 *
 * INIT_SNOW_PARTICLES (from the SNOW, LIGHT_SNOW and HEAVY_SNOW commands)
 * scatters 64 flakes around the origin; INIT_RAIN_PARTICLES (from RAIN,
 * LIGHT_RAIN and HEAVY_RAIN) scatters 32 streak end pairs over the map extent
 * and records the lowest tile surface (largest y) in
 * g_map_lowest_surface_y. The rain UV
 * pointer is formed from one combined byte offset: the target adds the
 * buffer/primitive offset it already keeps for the field stores to the pool
 * base instead of reusing the SetSemiTrans argument. */
static void init_weather_particles(weather_command_e command) {
    RECT rects[3] = { { 16, 160, 15, 15 }, { 32, 160, 15, 15 }, { 48, 160, 15, 15 } };
    s16 sprite;
    s16 tile;
    u32 i;
    u32 j;
    s16 surface_y;
    POLY_FT3* prim;

    switch (command & 0xff) {
    case WEATHER_COMMAND_INIT_SNOW_PARTICLES:
        g_weather_effect_mode = WEATHER_EFFECT_SNOW;
        for (i = 0; i < 64; i++) {
            g_weather_particles[i].y = -rand() % 240 - 16;
            g_weather_particles[i].fall_speed = s_weather_fall_speed;
            sprite = rand() % 3;
            for (j = 0; j < 2; j++) {
                SetSemiTrans(&g_render_buffers[j].ft3[i], 0);
                g_render_buffers[j].ft3[i].r0 = 0x80;
                g_render_buffers[j].ft3[i].g0 = 0x80;
                g_render_buffers[j].ft3[i].b0 = 0x80;
                g_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_render_buffers[j].ft3[i].u0 = rects[sprite].x;
                g_render_buffers[j].ft3[i].u1 = rects[sprite].x + rects[sprite].w;
                g_render_buffers[j].ft3[i].u2 = rects[sprite].x + rects[sprite].w;
                g_render_buffers[j].ft3[i].v0 = rects[sprite].y;
                g_render_buffers[j].ft3[i].v1 = rects[sprite].y;
                g_render_buffers[j].ft3[i].v2 = rects[sprite].y + rects[sprite].h;
            }
            g_weather_particles[i].fall_speed = sprite + s_weather_fall_speed;
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
    case WEATHER_COMMAND_INIT_RAIN_PARTICLES:
        g_weather_effect_mode = WEATHER_EFFECT_RAIN;
        for (i = 0; i < 32; i++) {
            g_weather_particles[i].x = rand() % (g_map_tile_width * MAP_TILE_SIZE);
            g_weather_particles[i].z = rand() % (g_map_tile_depth * MAP_TILE_SIZE);
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
                g_render_buffers[j].ft3[i].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                prim = (POLY_FT3*)((u8*)g_render_buffers[0].ft3 + (j * sizeof(render_buffer_t) + i * sizeof(POLY_FT3)));
                setUV3(prim, 216, 175, 247, 175, 216, 175);
                SetSemiTrans(&g_render_buffers[j].ft3[i + 32], 1);
                g_render_buffers[j].ft3[i + 32].r0 = 0x80;
                g_render_buffers[j].ft3[i + 32].g0 = 0x80;
                g_render_buffers[j].ft3[i + 32].b0 = 0x80;
                g_render_buffers[j].ft3[i + 32].clut = GetClut(0, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
                g_render_buffers[j].ft3[i + 32].tpage = GetTPage(0, 3, 0x3c0, 0x100);
                g_render_buffers[j].ft3[i + 32].u0 = 0x10;
                g_render_buffers[j].ft3[i + 32].u1 = 0x1f;
                g_render_buffers[j].ft3[i + 32].v0 = 0xa0;
                g_render_buffers[j].ft3[i + 32].u2 = 0x10;
                g_render_buffers[j].ft3[i + 32].v1 = 0xaf;
                g_render_buffers[j].ft3[i + 32].v2 = 0xaf;
            }
        }
        g_map_lowest_surface_y = 0x80000000;
        g_map_tile_data_ptr = g_map_tile_data;
        for (i = 0; i < g_map_tile_width; i++) {
            for (j = 0; j < g_map_tile_depth; j++) {
                tile = i + j * g_map_tile_width;
                surface_y = -((g_map_tile_data_ptr[tile].height + (g_map_tile_data_ptr[tile].depth_half_height.depth))
                    * MAP_HEIGHT_STEP);
                if (g_map_lowest_surface_y < surface_y) {
                    g_map_lowest_surface_y = surface_y;
                }
            }
        }
        break;
    }
}

/* map commands and animation */

/* Switch the ordering-table placement of untextured map polygons around an
 * ability effect: BEGIN places them by depth (OT z mask -1, fixed index 0), END
 * returns them to fixed slot 4 (mask 0), the values a map load sets. Returns 4
 * for END, and -1 for BEGIN or any other command. */
s32 map_set_untextured_ot_mode(map_effect_data_command_e command) {
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
 * map_draw.
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
static void update_mesh_animations(void) {
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
            g_map_mesh_parts[i].elapsed += (g_map_mesh_parts[i].speed >> 4) * g_system_vsync_interval;
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

                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_X] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vx
                        = s_map_mesh_animation_keyframes[anim].rotation.vx - g_map_mesh_parts[i].rotation_start.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Y] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vy
                        = s_map_mesh_animation_keyframes[anim].rotation.vy - g_map_mesh_parts[i].rotation_start.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Z] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].rotation_step.vz
                        = s_map_mesh_animation_keyframes[anim].rotation.vz - g_map_mesh_parts[i].rotation_start.vz;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_X]
                    & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vx = s_map_mesh_animation_keyframes[anim].translation.vx
                        - g_map_mesh_parts[i].translation_start.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Y]
                    & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vy = s_map_mesh_animation_keyframes[anim].translation.vy
                        - g_map_mesh_parts[i].translation_start.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Z]
                    & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].translation_step.vz = s_map_mesh_animation_keyframes[anim].translation.vz
                        - g_map_mesh_parts[i].translation_start.vz;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_X] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vx
                        = s_map_mesh_animation_keyframes[anim].scale.vx - g_map_mesh_parts[i].scale_start.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Y] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vy
                        = s_map_mesh_animation_keyframes[anim].scale.vy - g_map_mesh_parts[i].scale_start.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Z] & MAP_MESH_TWEEN_TO) {
                    g_map_mesh_parts[i].scale_step.vz
                        = s_map_mesh_animation_keyframes[anim].scale.vz - g_map_mesh_parts[i].scale_start.vz;
                }

                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_X] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vx = s_map_mesh_animation_keyframes[anim].rotation.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Y] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vy = s_map_mesh_animation_keyframes[anim].rotation.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Z] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].rotation_step.vz = s_map_mesh_animation_keyframes[anim].rotation.vz;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_X]
                    & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vx = s_map_mesh_animation_keyframes[anim].translation.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Y]
                    & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vy = s_map_mesh_animation_keyframes[anim].translation.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Z]
                    & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].translation_step.vz = s_map_mesh_animation_keyframes[anim].translation.vz;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_X] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vx = s_map_mesh_animation_keyframes[anim].scale.vx;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Y] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vy = s_map_mesh_animation_keyframes[anim].scale.vy;
                }
                if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Z] & MAP_MESH_TWEEN_BY) {
                    g_map_mesh_parts[i].scale_step.vz = s_map_mesh_animation_keyframes[anim].scale.vz;
                }
            }

            /* Evaluate each channel's delta at elapsed / duration on its
             * curve: linear, sine or raised sine. */
            anim = g_map_mesh_parts[i].keyframe;
            duration = g_map_mesh_parts[i].duration;
            time = g_map_mesh_parts[i].elapsed;
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_X] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Y] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Z] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_X]
                & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Y]
                & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Z]
                & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_X] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Y] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy * time / duration;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Z] & MAP_MESH_TWEEN_LINEAR) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz * time / duration;
            }

            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_X] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_X]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_X])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_X])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Y] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_Y]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Y])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Y])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Z] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_Z]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Z])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Z])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_X]
                & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_X]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_X])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_X])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Y]
                & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_Y]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Y])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Y])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Z]
                & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_Z]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Z])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Z])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_X] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_X]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_X])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_X])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Y] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_Y]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Y])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Y])
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Z] & MAP_MESH_TWEEN_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_Z]
                                   - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Z])
                                * time / duration
                            + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Z])
                    >> 12;
            }

            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_X]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vx = g_map_mesh_parts[i].rotation_step.vx
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_X]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_X])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_X])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Y]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vy = g_map_mesh_parts[i].rotation_step.vy
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_Y]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Y])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Y])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_ROTATION_Z]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].rotation.vz = g_map_mesh_parts[i].rotation_step.vz
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_ROTATION_Z]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Z])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_ROTATION_Z])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_X]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vx = g_map_mesh_parts[i].translation_step.vx
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_X]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_X])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_X])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Y]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vy = g_map_mesh_parts[i].translation_step.vy
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_Y]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Y])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Y])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_TRANSLATION_Z]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].translation.vz = g_map_mesh_parts[i].translation_step.vz
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_TRANSLATION_Z]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Z])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_TRANSLATION_Z])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_X]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vx = g_map_mesh_parts[i].scale_step.vx
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_X]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_X])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_X])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Y]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vy = g_map_mesh_parts[i].scale_step.vy
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_Y]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Y])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Y])
                            + ONE)
                    >> 12;
            }
            if (s_map_mesh_animation_keyframes[anim].tween_flags[MAP_MESH_CHANNEL_SCALE_Z]
                & MAP_MESH_TWEEN_RAISED_SINE) {
                g_map_mesh_parts[i].scale.vz = g_map_mesh_parts[i].scale_step.vz
                        * (rsin((s_map_mesh_animation_keyframes[anim].angle_end[MAP_MESH_CHANNEL_SCALE_Z]
                                    - s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Z])
                                   * time / duration
                               + s_map_mesh_animation_keyframes[anim].angle_start[MAP_MESH_CHANNEL_SCALE_Z])
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
 * duration. Only the two GET commands set the result; the others return the
 * unset local. */
s32 map_dispatch_data_command(map_data_command_e command, u32 index, s32 object_state, s32 flag) {
    s32 result;
    s16 i;
    mesh_instruction_list_t* entry;

    switch (command) {
    case MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE:
        result = g_map_texture_animations[index].active;
        break;
    case MAP_DATA_COMMAND_START_TEXTURE_ANIMATION:
        if (g_map_texture_animations[index].mode == MAP_TEXTURE_ANIMATION_MODE_NONE) {
            if (s_saved_texture_modes[index] != 0) {
                g_map_texture_animations[index].mode = s_saved_texture_modes[index];
            }
        }
        switch (g_map_texture_animations[index].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP ... MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK ... MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            s_texture_animation_moves[index].sx = g_map_texture_animations[index].frame_x;
            s_texture_animation_moves[index].sy = g_map_texture_animations[index].frame_y;
            s_texture_animation_moves[index].w = g_map_texture_animations[index].width;
            s_texture_animation_moves[index].h = g_map_texture_animations[index].height;
            s_texture_animation_moves[index].x0 = g_map_texture_animations[index].canvas_x;
            s_texture_animation_moves[index].y0 = g_map_texture_animations[index].canvas_y;
            g_map_texture_animations[index].active = 1;
            g_map_texture_animations[index].timer = 0x7e;
            g_map_texture_animations[index].frame = 0;
            break;
        default:
            system_report_error(2, 0x3cd);
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
        /* Cut the primary mesh's polygons of each class into eight equal runs for
         * parts 1-8 (the last may run past the end) and leave part 0 empty. */
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
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    case MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER:
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    case MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            s_saved_texture_modes[i] = g_map_texture_animations[i].mode;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS:
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            g_map_texture_animations[i].mode = s_saved_texture_modes[i];
        }
        break;
    case MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            s_saved_texture_modes[index] = g_map_texture_animations[index].mode;
            g_map_texture_animations[index].mode = MAP_TEXTURE_ANIMATION_MODE_NONE;
        }
        break;
    case MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION:
        if (index < MAP_TEXTURE_ANIMATION_COUNT) {
            g_map_texture_animations[index].mode = s_saved_texture_modes[index];
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
 * Only the low byte of g_system_vsync_interval is read (lbu), and the start call
 * passes the color range unmasked, as the target does.
 *
 * The two loop modes count frames and step once the timer reaches the
 * duration; the other modes count vertical blanks and step once it exceeds it.
 */
static void update_texture_animations(void) {
    s32 i;

    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        if (!g_map_texture_animations[i].active) {
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
            /* Past the right edge of the 64-wide texture page, continue at the start of the next row. */
            if (s_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row++;
                s_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
            }
            if (g_map_texture_animations[i].frame_count - 1 < g_map_texture_animations[i].frame) {
                g_map_texture_animations[i].frame = 0;
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row = 0;
                s_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x - g_map_texture_animations[i].width;
            }

            /* Aim the DR_MOVE at the new frame and queue its copy onto the canvas. */
            s_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
            s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
            AddPrim(g_render_current_buffer->otag, &s_texture_animation_moves[i]);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG:
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (s_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    s_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame_count - 2 < g_map_texture_animations[i].frame) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK;
                }

                s_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &s_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                /* Before the page's left edge, go back to the last column of the previous row. */
                if (s_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width;
                    g_map_texture_animations[i].row--;
                    s_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].column * g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG;
                }

                s_texture_animation_moves[i].sx -= g_map_texture_animations[i].width;
                s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &s_texture_animation_moves[i]);
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

            map_palette_load_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].column,
                MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG:
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count - 1) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK;
                }
                map_palette_load_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG;
                }
                map_palette_load_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
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
            /* Walk the packet to the last frame, then play backward from it. */
            g_map_texture_animations[i].frame = 0;
            while (g_map_texture_animations[i].frame < g_map_texture_animations[i].frame_count) {
                g_map_texture_animations[i].column++;
                if (s_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    s_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                s_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                g_map_texture_animations[i].frame++;
            }

            g_map_texture_animations[i].timer = 0x7E;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING;
            /* fall through */
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                if (s_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width - 1;
                    g_map_texture_animations[i].row--;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].active = 0;
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0;
                }

                s_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].width * g_map_texture_animations[i].column;
                s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_render_current_buffer->otag, &s_texture_animation_moves[i]);
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
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                if (g_map_texture_animations[i].column != 0) {
                    s_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                }
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (s_texture_animation_moves[i].sx + g_map_texture_animations[i].width
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    s_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    s_texture_animation_moves[i].sy
                        += g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                }

                if (g_map_texture_animations[i].frame > g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].frame = 0;
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row = 0;
                    g_map_texture_animations[i].active = 0;
                    s_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x;
                    s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_render_current_buffer->otag, &s_texture_animation_moves[i]);
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
            g_map_texture_animations[i].timer += g_system_vsync_interval;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                map_palette_load_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].active = 0;
                }
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
            /* Start the color rotation once; map_palette_update_color_animations runs it from then on. */
            if (s_map_texture_animation_cycle_handles[i] == 0) {
                s_map_texture_animation_cycle_handles[i] = map_palette_start_clut_cycle(
                    g_map_texture_animations[i].duration, MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x,
                    g_map_texture_animations[i].canvas_x + g_map_texture_animations[i].width - 1);
                /* All eight rotation slots are in use. */
                if (s_map_texture_animation_cycle_handles[i] == 0) {
                    system_report_error(2, 0x3CC);
                }
            }
            break;
        }
    }
}

/* GNS resources */

/* Loads one record of the saved map state and returns the address just past it.
 *
 * Type 1 fills entries 1..count of the 0x50-byte table, type 2 the 16 cells of
 * each of 8 rows for entries 1..8 of the 0x220-byte table, and type 3 sets up
 * the 0x98-byte entries 1..8 from the other two tables (4 input bytes per
 * entry, extra entries skipped). The caller stops at type 0 before calling.
 * An unknown nonzero type leaves `next` unset (the target returns $a1), yet
 * the caller uses that result to read the next record's type. */
static map_state_record_t* load_mesh_animation_record(map_state_record_t* record) {
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
            s_map_mesh_animation_keyframes[i] = *src++;
        }
        next = (map_state_record_t*)src;
        break;
    }
    case MAP_MESH_ANIMATION_RECORD_END:
        break;
    case MAP_MESH_ANIMATION_RECORD_INSTRUCTIONS: {
        mesh_instruction_t* src = (mesh_instruction_t*)record->data;

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
        map_mesh_part_property_t* src = (map_mesh_part_property_t*)record->data;

        count++;
        for (i = 1; i < count; i++) {
            if (i >= 9) {
                break;
            }
            g_map_mesh_parts[i].parent = src->parent;
            g_map_mesh_parts[i].duration = 0;
            g_map_mesh_parts[i].elapsed = 2;
            src++;
            if (g_map_mesh_animation_instructions[i].states[0][1].next != 0) {
                g_map_mesh_part_animation_states[i] = 1;
            } else {
                g_map_mesh_part_animation_states[i] = 0;
            }
            g_map_mesh_parts[i].keyframe = 0;
            g_map_mesh_parts[i].rotation
                = s_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].rotation;
            g_map_mesh_parts[i].translation
                = s_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe]
                      .translation;
            g_map_mesh_parts[i].scale.vx
                = s_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale.vx;
            g_map_mesh_parts[i].scale.vy
                = s_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale.vy;
            g_map_mesh_parts[i].scale.vz
                = s_map_mesh_animation_keyframes[g_map_mesh_animation_instructions[i].states[0][1].keyframe].scale.vz;
        }
        for (; i < count; i++) {
            src++;
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
static s16 gns_record_matches_state(s32 comparison, s16 record_state, s16 requested_state) {
    s32 matches;

    comparison &= 0xff;
    matches = 0;
    switch (comparison) {
    case GNS_MAP_STATE_EQUAL:
        if (requested_state == record_state) {
            matches = 1;
        }
        break;
    case GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD:
        if (requested_state < record_state) {
            matches = 1;
        }
        break;
    case GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD:
        if (requested_state > record_state) {
            matches = 1;
        }
        break;
    }
    return matches;
}

/* Apply one map resource by type: mesh parts, polygon flags, palettes,
 * textures, lighting, texture animations, terrain, saved state, camera and
 * weather commands. The three mesh file types walk the file's table of chunk
 * offsets and dispatch each present entry as types 0x10-0x30. */
static s32 dispatch_gns_resource(u8 resource_type, map_resource_t* data) {
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

    zero_vector = s_zero_svector;
    unit_scale = s_unit_scale_vector;

    /* Let queued drawing finish before a resource replaces data the GPU may still be reading. */
    while (DrawSync(1)) { }
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
            s_map_mesh_part_geometry[i] = 0;
        }

        s_map_mesh_part_geometry[0] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[0]);

        /* The counts after part 0 alone are kept as the primary mesh's. */
        g_map_primary_textured_triangle_count = g_map_textured_triangle_count;
        g_map_primary_textured_quad_count = g_map_textured_quad_count;
        g_map_primary_untextured_triangle_count = g_map_untextured_triangle_count;
        g_map_primary_untextured_quad_count = g_map_untextured_quad_count;
        break;
    case MAP_RESOURCE_GEOMETRY_PART_1:
        s_map_mesh_part_geometry[1] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[1]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_2:
        s_map_mesh_part_geometry[2] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[2]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_3:
        s_map_mesh_part_geometry[3] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[3]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_4:
        s_map_mesh_part_geometry[4] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[4]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_5:
        s_map_mesh_part_geometry[5] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[5]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_6:
        s_map_mesh_part_geometry[6] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[6]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_7:
        s_map_mesh_part_geometry[7] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[7]);
        break;
    case MAP_RESOURCE_GEOMETRY_PART_8:
        s_map_mesh_part_geometry[8] = &data->geometry;
        append_mesh_geometry(&data->geometry, &g_map_mesh_parts[8]);
        break;
    case MAP_RESOURCE_POLYGON_FLAGS:
        polygon_flags = data->polygon_flags.textured_triangles;
        for (i = 0; i < RENDER_TEXTURED_TRIANGLE_COUNT; i++) {
            flags = *polygon_flags++;
            g_map_textured_triangle_positions[i].polygon_flags = flags;
            /* An unlit polygon starts at the neutral color 0x80 in both render buffers. */
            if (!(flags & MAP_POLYGON_FLAG_LIT)) {
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
            g_map_textured_quad_positions[i].polygon_flags = flags;
            if (!(flags & MAP_POLYGON_FLAG_LIT)) {
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
            g_map_untextured_triangle_positions[i].polygon_flags = *polygon_flags++;
        }
        for (i = 0; i < RENDER_UNTEXTURED_QUAD_COUNT; i++) {
            g_map_untextured_quad_positions[i].polygon_flags = *polygon_flags++;
        }

        dispatch_polygon_flag_command(MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE);
        break;
    case MAP_RESOURCE_PALETTES:
        g_map_palette_pending = 1;
        for (i = 0; i < 0x200; i++) {
            g_map_pending_palette_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_PALETTES:
        g_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MAP;
        g_map_vram_load_rect.w = 256;
        g_map_palette_pending = 0;
        g_map_vram_load_rect.x = 0;
        g_map_vram_load_rect.h = 1;
        map_palette_load_data(data->clut, MAP_CLUT_ROW_MAP, 0, 1);
        break;
    case MAP_RESOURCE_MARKED_TILE_PALETTES:
        g_map_vram_load_rect.y = MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_MARKED_TILES;
        g_map_vram_load_rect.w = 256;
        g_map_vram_load_rect.x = 0;
        g_map_vram_load_rect.h = 1;
        map_palette_load_data(data->clut, MAP_CLUT_ROW_MARKED_TILES, 0, 1);
        break;
    case MAP_RESOURCE_UNSUPPORTED_12:
        system_report_error(2, 0x3ca);
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_0:
        g_map_vram_load_rect.x = 768;
        g_map_vram_load_rect.w = 64;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 256;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1)) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_1:
        g_map_vram_load_rect.x = 832;
        g_map_vram_load_rect.w = 64;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 256;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1)) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_2:
        g_map_vram_load_rect.x = 896;
        g_map_vram_load_rect.w = 64;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 256;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1)) { }
        break;
    case MAP_RESOURCE_TEXTURE_QUARTER_3:
        g_map_vram_load_rect.x = 960;
        g_map_vram_load_rect.w = 64;
        g_map_vram_load_rect.y = 0;
        g_map_vram_load_rect.h = 256;
        LoadImage(&g_map_vram_load_rect, data->texture_words);
        while (DrawSync(1)) { }
        break;
    case MAP_RESOURCE_TEXTURE:
        /* A full texture is four 0x8000-byte quarters, uploaded side by side from VRAM x 0x300. */
        if (g_map_texture_quarter_0_pending) {
            dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_0, data);
        }
        if (g_map_texture_quarter_1_pending) {
            dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_1, (map_resource_t*)(data->bytes + 0x8000));
        }
        if (g_map_texture_quarter_2_pending) {
            dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_2, (map_resource_t*)(data->bytes + 0x10000));
        }
        if (s_map_texture_quarter_3_pending) {
            dispatch_gns_resource(MAP_RESOURCE_TEXTURE_QUARTER_3, (map_resource_t*)(data->bytes + 0x18000));
        }

        g_map_texture_quarter_0_pending = 0;
        g_map_texture_quarter_1_pending = 0;
        g_map_texture_quarter_2_pending = 0;
        s_map_texture_quarter_3_pending = 0;
        g_map_texture_pending = 0;
        break;
    case MAP_RESOURCE_MESH_PRIMARY:
    case MAP_RESOURCE_MESH_OVERRIDE:
    case MAP_RESOURCE_MESH_ALTERNATE:
        for (i = MAP_RESOURCE_GEOMETRY_PRIMARY; i < GNS_RESOURCE_TABLE_COUNT; i++) {
            if (data->mesh_file[i]) {
                dispatch_gns_resource(i, (map_resource_t*)(data->bytes + data->mesh_file[i]));
            }
        }
        break;
    case MAP_RESOURCE_LIGHTING:
        g_map_lighting_pending = 1;
        for (i = 0; i < 45; i++) {
            g_map_pending_lighting_data[i] = data->bytes[i];
        }
        break;
    case MAP_RESOURCE_APPLY_LIGHTING:
        light = &data->lighting;
        g_map_lighting_pending = 0;
        setVector(&g_map_light_rotation, 0, 0, 0);

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
        set_light_matrices((MATRIX*)light, &direction);
        map_light_dispatch_back_color_command(
            MAP_LIGHT_BACK_COLOR_SET, light->ambient.r, light->ambient.g, light->ambient.b);

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

        /* Lightning flashes fade back to these colors. */
        s_weather_lightning_state.base_colors[0] = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_TOP];
        s_weather_lightning_state.base_colors[1]
            = g_map_background_gradient_colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
        map_light_dispatch_command(MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT, 0);
        break;
    case MAP_RESOURCE_TEXTURE_ANIMATIONS:
        source = data->texture_animations;
        for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
            s_map_texture_animation_cycle_handles[i] = 0;
            g_map_texture_animations[i] = source[i];
            SetDrawMove(&s_texture_animation_moves[i]);
            s_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x;
            s_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y;
            s_texture_animation_moves[i].w = g_map_texture_animations[i].width;
            s_texture_animation_moves[i].h = g_map_texture_animations[i].height;
            s_texture_animation_moves[i].x0 = g_map_texture_animations[i].canvas_x;
            s_texture_animation_moves[i].y0 = g_map_texture_animations[i].canvas_y;
            /* Play-once modes wait for map_dispatch_data_command to start them; the rest run at once. */
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
        map_copy_tile_data(data->bytes);
        break;
    }
    case MAP_RESOURCE_STATE_RECORDS:
        cursor = &data->state_record;
        while (cursor->type != MAP_MESH_ANIMATION_RECORD_END) {
            cursor = load_mesh_animation_record(cursor);
        }

        break;
    case GNS_COMMAND_SET_CAMERA_POSITION:
        /* Halfword 6 is the payload vector at +0x0c. */
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_camera_integer_coords, -vector[0], vector[1], -vector[2]);
        camera_set_position_and_cursor(&g_camera_integer_coords, &g_map_camera_rotation);
        break;
    case GNS_COMMAND_SET_CAMERA_ROTATION:
        vector = data->command_halfwords;
        vector += 6;
        setVector(&g_map_camera_rotation, vector[0], vector[1], vector[2]);
        camera_set_position_and_cursor(&g_camera_integer_coords, &g_map_camera_rotation);
        break;
    /* Case-local pointers preserve the native address lifetimes. */
    case GNS_COMMAND_SET_INDOOR: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags |= WEATHER_FLAG_IGNORE_WEATHER;
        /* Indoors the backdrop gradient is drawn without dithering. */
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 0, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 0, 0, 0);
        break;
    }
    case GNS_COMMAND_SET_OUTDOOR: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags &= ~WEATHER_FLAG_IGNORE_WEATHER;
        SetDrawMode(&g_render_buffers[0].overlay_modes[0], 0, 1, 0, 0);
        SetDrawMode(&g_render_buffers[1].overlay_modes[0], 0, 1, 0, 0);
        break;
    }
    case GNS_COMMAND_CLEAR_SNOW: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags &= ~WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_SNOW: {
        s32* weather_flags;

        weather_flags = &g_weather_flags;
        *weather_flags |= WEATHER_FLAG_SNOW;
        break;
    }
    case GNS_COMMAND_SET_WEATHER_MODIFIER: {
        s32* weather_flags;

        vector = data->command_halfwords;
        vector += 6;
        weather_flags = &g_weather_flags;
        *weather_flags = (*weather_flags & WEATHER_LOW_FLAGS_MASK) | (vector[0] << WEATHER_MODIFIER_SHIFT);
        break;
    }
    }
    return result;
}

/* Apply one command record that follows the resource rows of a map's GNS file and
 * step g_map_gns_record_cursor past it.
 *
 * The indoor, outdoor, snow, 0x8a and weather-modifier commands always run. Any
 * other command runs only when the requested map state compares with the record's
 * map state as its comparison asks. The requested state is the low 12 bits of the
 * script variable the record names (the layout) packed with the script's weather
 * and time of day. Only the low byte of the record's variable id and of its map
 * state is read.
 *
 * Rows are applied in file order, so the last row of a kind wins; type 0x87 has
 * no dispatcher case.
 *
 * Only phase 2 does anything, and it is the only phase the map loader passes.
 * Always returns 0. */
static s32 apply_gns_command(s32 phase, map_resource_t* record) {
    /* Copied and never used, as in dispatch_gns_resource. */
    SVECTOR zero_vector = s_zero_svector;
    VECTOR unit_scale = s_unit_scale_vector;
    s32 requested_layout;
    s32 requested_layout_and_weather;
    map_resource_t* selected;

    if (phase == 2) {
        /* Everything below works on this copy; the parameter itself dies
         * here, which keeps it out of a callee-saved register. */
        selected = record;
        if (selected->command.resource_type == GNS_COMMAND_SET_INDOOR) {
            dispatch_gns_resource(GNS_COMMAND_SET_INDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_OUTDOOR) {
            dispatch_gns_resource(GNS_COMMAND_SET_OUTDOOR, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_CLEAR_SNOW) {
            dispatch_gns_resource(GNS_COMMAND_CLEAR_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_SNOW) {
            dispatch_gns_resource(GNS_COMMAND_SET_SNOW, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_UNHANDLED_8A) {
            dispatch_gns_resource(GNS_COMMAND_UNHANDLED_8A, selected);
        } else if (selected->command.resource_type == GNS_COMMAND_SET_WEATHER_MODIFIER) {
            dispatch_gns_resource(GNS_COMMAND_SET_WEATHER_MODIFIER, selected);
        } else {
            /*
             * Requested map state, packed like the file row's map_state: layout in
             * bits 0-11, weather in 12-14, time of day in 15. Retail truncates
             * the record's variable id and map state to their low bytes.
             */
            requested_layout = script_variable_get((u8)selected->command.script_variable_id) & 0xfff;
            requested_layout_and_weather = requested_layout | ((script_variable_get(SCRIPT_VAR_WEATHER) & 7) << 12);
            if (gns_record_matches_state(selected->command.map_state_comparison, (u8)selected->command.map_state,
                    requested_layout_and_weather | ((script_variable_get(SCRIPT_VAR_TIME_OF_DAY) & 1) << 15))) {
                dispatch_gns_resource(selected->command.resource_type, selected);
            }
        }
        g_map_gns_record_cursor += selected->command.record_byte_length;
    }
    return 0;
}

enum { GNS_INITIAL_LOAD_BYTES = 0x1000, GNS_BUFFER_CLEAR_BYTES = 0xbb8 };

/* Make a map current for loading: copy its move-find item record, clear the
 * 150-row GNS table and start reading the first 0x1000 bytes of its GNS file into
 * the load buffer.
 *
 * Returns the GNS table when the read started and 0 when it could not start. A
 * map without a GNS sector returns whatever bzero left in $v0. */
static map_gns_record_t* start_gns_load(s32 map_id) {
    u32 selected_map = map_id & 0xffff;
    map_gns_record_t* gns_records;
    u32 lba;

    move_find_load_item_data(&g_move_find_map_data[selected_map]);

    gns_records = g_map_gns_records;
    bzero(gns_records, GNS_BUFFER_CLEAR_BYTES);

    lba = g_map_gns_lba[selected_map];
    if (lba != 0) {
        if (file_request_read_bytes(lba, GNS_INITIAL_LOAD_BYTES, g_map_data_load_buffer) != 0) {
            return 0;
        }
        return gns_records;
    }
    /* The target falls through with the BIOS bzero result still in v0. */
}

/* Start reading one map file into `destination`. Returns `destination` when
 * the read was started, 0 when the row names no file or the start failed. */
static void* start_file_load(const char* unused_directory_path, const map_gns_load_info_t* load_info, void* destination,
    s32 unused_resource_type) {
    void* result;

    if (load_info->fields.file_present_marker == 0) {
        result = 0;
    } else {
        /* These 32-bit values are aligned within the file row; retail loads both with lw. */
        if (file_request_read_bytes(
                *(const s32*)load_info->fields.disc_lba_le, *(const s32*)load_info->fields.byte_length_le, destination)
            == 0) {
            return destination;
        }
        result = 0;
    }
    return result;
}

/* loading */

/* The PC-side staging directory of the GNS files; start_file_load ignores it. */
static const char s_map_directory_path[] = "C:\\FFTIMG\\MAP\\GNS\\";

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
 * - Selection: fill s_map_pending_gns_records, indexed by resource type.
 *   A row's comparison is tested against the low 12 bits of its script
 *   variable with the weather in bits 12-14 and the time of day in bit 15. A row
 *   is taken when its file bytes differ from the row last loaded for its type.
 *   Four passes: primary meshes, override meshes, the default texture, then
 *   every other row whose comparison holds.
 * - Resources: read the pending rows one file at a time in type order and hand
 *   each to dispatch_gns_resource. Types 0x12-0x17 come last: 0x12
 *   reports error 0x3cb (to the empty system_report_error), the texture
 *   quarters load without being applied
 *   here, and the texture is followed by the pending lighting, the texture
 *   upload and the pending palettes.
 * - Mesh records: append_mesh_geometry requests them; the render
 *   records of the nine mesh parts are rebuilt once in each render buffer.
 * - End: run the command records after the file rows, apply the script
 *   weather, free the buffer.
 *
 * Returns g_map_gns_load_phase: nonzero while loading, 0 when done or
 * idle. A command that cannot start because a file read is in progress also
 * returns 0, and a loaded file held by START_HELD returns MAP_LOAD_RESULT_HELD
 * until MAP_DATA_COMMAND_RELEASE_GNS_HOLD. A failed buffer allocation is only
 * reported (to the same empty hook) and the load goes on with a null buffer.
 *
 * Texture rows are never copied to g_map_loaded_gns_records, so a selected
 * texture is read again on every start. The fourth pass does not exclude the
 * filler rows (type 0x31): one whose comparison holds reads and writes entry 49
 * of the two 49-entry tables. Quarter n loads at buffer + 0x8000 * (n + 1), one
 * quarter above where the dispatcher uploads it from, and quarter 3 lies past the
 * end of the buffer; retail GNS files hold no quarter rows. */
s32 map_update_load(s32 map_id, map_load_command_e command) {
    s32 i;
    s32 r;
    s32 j;
    s32 k;
    s16 state;
    u8 diff;

    /* A command is taken only between loads. */
    if (g_map_gns_load_phase == MAP_GNS_LOAD_PHASE_IDLE) {
        switch (command) {
        case MAP_LOAD_COMMAND_LOAD_GNS:
            if (file_get_load_state() != 0) {
                return 0;
            }

            g_map_data_load_buffer = heap_alloc(0x20000);
            if (g_map_data_load_buffer == 0) {
                system_report_error(2, 0x3c2);
            }

            start_gns_load(map_id & 0xffff);
            g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
            g_map_loaded_id = map_id;
            g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
            reset_render_state();
            break;
        case MAP_LOAD_COMMAND_START_HELD:
            g_map_gns_dispatch_held = 1;
            /* fall through */
        case MAP_LOAD_COMMAND_START:
            if (file_get_load_state() != 0) {
                return 0;
            }

            g_map_data_load_buffer = heap_alloc(0x20000);
            if (g_map_data_load_buffer == 0) {
                system_report_error(2, 0x3c2);
            }

            g_map_texture_quarter_0_pending = 0;
            g_map_texture_quarter_1_pending = 0;
            g_map_texture_quarter_2_pending = 0;
            s_map_texture_quarter_3_pending = 0;
            g_map_texture_pending = 0;
            g_map_palette_pending = 0;
            g_map_lighting_pending = 0;

            if (map_id != g_map_loaded_id) {
                if (start_gns_load(map_id & 0xffff) != g_map_gns_records) {
                    return 0;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_GNS_FILE;
                g_map_loaded_id = map_id;
                g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_START;
                reset_render_state();
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
                for (k = 0; k < GNS_RESOURCE_TABLE_COUNT; k++) {
                    s_map_pending_gns_records[k].resource_type = GNS_RECORD_NONE;
                }

                /* Pass 1: primary meshes. A row whose comparison holds forgets
                 * what is loaded for its type, so it is always read again. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_MESH_PRIMARY) {
                        state = script_variable_get(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_variable_get(SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_variable_get(SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (gns_record_matches_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            /* No braces: a braced body's block note lengthens this loop by one
                             * luid, and loop.c then hoists the 0x48 out of the record loop. */
                            j = 0;
                            do
                                g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[j] = 0x48;
                            while (++j < 14);
                        }

                        diff = 0;
                        for (k = 0; k < 14; k++) {
                            diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                ^ g_map_gns_records[r].load_info.bytes[k];
                        }
                        if (diff) {
                            s_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }

                /* Pass 2: override meshes, by the same rule. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_MESH_OVERRIDE) {
                        state = script_variable_get(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_variable_get(SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_variable_get(SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (gns_record_matches_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            j = 0;
                            do
                                g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[j] = 0x48;
                            while (++j < 14);
                        }

                        diff = 0;
                        for (k = 0; k < 14; k++) {
                            diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                ^ g_map_gns_records[r].load_info.bytes[k];
                        }
                        if (diff) {
                            s_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }

                /* Pass 3: the default texture (state 0 of the two map variables). */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type == MAP_RESOURCE_TEXTURE
                        && (g_map_gns_records[r].script_variable_id == SCRIPT_VAR_WEATHER_AND_TIME
                            || g_map_gns_records[r].script_variable_id == SCRIPT_VAR_MAP_ARRANGEMENT)
                        && g_map_gns_records[r].map_state == 0) {
                        diff = 0;
                        for (k = 0; k < 14; k++) {
                            diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                ^ g_map_gns_records[r].load_info.bytes[k];
                        }
                        if (diff) {
                            s_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                        }
                    }
                }

                /* Pass 4: every other row whose comparison holds replaces the
                 * choice for its type, or cancels it when that file is loaded. */
                for (r = 0; g_map_gns_records[r].resource_type < GNS_RECORD_COMMAND_SEPARATOR; r++) {
                    if (g_map_gns_records[r].resource_type != GNS_RECORD_NONE
                        && (g_map_gns_records[r].script_variable_id != SCRIPT_VAR_WEATHER_AND_TIME
                            || g_map_gns_records[r].map_state != 0)
                        && (g_map_gns_records[r].script_variable_id != SCRIPT_VAR_MAP_ARRANGEMENT
                            || g_map_gns_records[r].map_state != 0)) {
                        state = script_variable_get(g_map_gns_records[r].script_variable_id) & 0xfff;
                        state |= (script_variable_get(SCRIPT_VAR_WEATHER) & 7) << 12;
                        state |= (script_variable_get(SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                        if (gns_record_matches_state(
                                g_map_gns_records[r].map_state_comparison, g_map_gns_records[r].map_state, state)) {
                            diff = 0;
                            for (k = 0; k < 14; k++) {
                                diff |= g_map_loaded_gns_records[g_map_gns_records[r].resource_type].load_info.bytes[k]
                                    ^ g_map_gns_records[r].load_info.bytes[k];
                            }
                            if (diff) {
                                s_map_pending_gns_records[g_map_gns_records[r].resource_type] = g_map_gns_records[r];
                            } else {
                                s_map_pending_gns_records[g_map_gns_records[r].resource_type].resource_type
                                    = GNS_RECORD_NONE;
                            }
                        }
                    }
                }
            }
        } else {
            /* Resources: start the next pending file. Types 0x12-0x17 are
             * passed over here and taken after the others. */
            if (file_get_load_state() == 0) {
                while ((s_map_pending_gns_records[g_map_gns_record_cursor].resource_type == GNS_RECORD_NONE
                           && g_map_gns_record_cursor < GNS_RESOURCE_TABLE_COUNT)
                    || (s_map_pending_gns_records[g_map_gns_record_cursor].resource_type >= 0x12
                        && s_map_pending_gns_records[g_map_gns_record_cursor].resource_type <= 0x17)) {
                    g_map_gns_record_cursor++;
                }

                if (g_map_gns_record_cursor < GNS_RESOURCE_TABLE_COUNT) {
                    /* Clear the 32 20-byte entries the dispatcher copies, so a shorter file leaves the rest zero. */
                    if (g_map_gns_record_cursor == MAP_RESOURCE_TEXTURE_ANIMATIONS) {
                        bzero(g_map_data_load_buffer, 0x280);
                    }
                    start_file_load(s_map_directory_path, &s_map_pending_gns_records[g_map_gns_record_cursor].load_info,
                        g_map_data_load_buffer, g_map_gns_record_cursor);
                    g_map_resource_load_state = MAP_LOAD_STATE_RESOURCE_FILE;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type != GNS_RECORD_NONE) {
                    system_report_error(2, 0x3cb);
                    s_map_pending_gns_records[MAP_RESOURCE_UNSUPPORTED_12].resource_type = GNS_RECORD_NONE;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type != GNS_RECORD_NONE) {
                    s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].resource_type = GNS_RECORD_NONE;
                    start_file_load(s_map_directory_path,
                        &s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_0].load_info,
                        g_map_data_load_buffer->bytes + 0x8000, MAP_RESOURCE_TEXTURE_QUARTER_0);
                    g_map_texture_quarter_0_pending = 1;
                    g_map_texture_pending = 1;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type != GNS_RECORD_NONE) {
                    s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].resource_type = GNS_RECORD_NONE;
                    start_file_load(s_map_directory_path,
                        &s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_1].load_info,
                        g_map_data_load_buffer->bytes + 0x10000, MAP_RESOURCE_TEXTURE_QUARTER_1);
                    g_map_texture_quarter_1_pending = 1;
                    g_map_texture_pending = 1;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type != GNS_RECORD_NONE) {
                    s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].resource_type = GNS_RECORD_NONE;
                    start_file_load(s_map_directory_path,
                        &s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_2].load_info,
                        g_map_data_load_buffer->bytes + 0x18000, MAP_RESOURCE_TEXTURE_QUARTER_2);
                    g_map_texture_quarter_2_pending = 1;
                    g_map_texture_pending = 1;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type != GNS_RECORD_NONE) {
                    s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].resource_type = GNS_RECORD_NONE;
                    start_file_load(s_map_directory_path,
                        &s_map_pending_gns_records[MAP_RESOURCE_TEXTURE_QUARTER_3].load_info,
                        g_map_data_load_buffer->bytes + 0x20000, MAP_RESOURCE_TEXTURE_QUARTER_3);
                    s_map_texture_quarter_3_pending = 1;
                    g_map_texture_pending = 1;
                } else if (s_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type != GNS_RECORD_NONE) {
                    s_map_pending_gns_records[MAP_RESOURCE_TEXTURE].resource_type = GNS_RECORD_NONE;
                    start_file_load(s_map_directory_path, &s_map_pending_gns_records[MAP_RESOURCE_TEXTURE].load_info,
                        g_map_data_load_buffer, MAP_RESOURCE_TEXTURE);
                    g_map_texture_quarter_0_pending = 1;
                    g_map_texture_quarter_1_pending = 1;
                    g_map_texture_quarter_2_pending = 1;
                    s_map_texture_quarter_3_pending = 1;
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
                        apply_gns_command(2, (map_resource_t*)g_map_gns_record_cursor);
                    }

                    g_map_gns_load_phase = MAP_GNS_LOAD_PHASE_IDLE;
                    g_map_resource_load_state = MAP_LOAD_STATE_DONE;
                    weather_dispatch_command(WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
                    heap_free(g_map_data_load_buffer);
                }
            }
        }
    } else {
        switch (g_map_resource_load_state) {
        case MAP_LOAD_STATE_GNS_FILE:
            if (file_get_load_state() == 0) {
                map_gns_record_t* source;
                map_gns_record_t* destination;

                /* The bytes require this do-while with the counter set first. */
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
            if (file_get_load_state() == 0) {
                if (g_map_gns_dispatch_held) {
                    return MAP_LOAD_RESULT_HELD;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
                dispatch_gns_resource(
                    s_map_pending_gns_records[g_map_gns_record_cursor].resource_type, g_map_data_load_buffer);
                g_map_loaded_gns_records[g_map_gns_record_cursor] = s_map_pending_gns_records[g_map_gns_record_cursor];
                g_map_gns_record_cursor++;
            }
            break;
        case MAP_LOAD_STATE_APPLY_LIGHTING:
            if (file_get_load_state() == 0) {
                if (g_map_lighting_pending) {
                    dispatch_gns_resource(MAP_RESOURCE_APPLY_LIGHTING, (map_resource_t*)g_map_pending_lighting_data);
                }
                g_map_lighting_pending = 0;
                g_map_resource_load_state = MAP_LOAD_STATE_APPLY_TEXTURE;
            }
            break;
        case MAP_LOAD_STATE_APPLY_TEXTURE:
            if (g_map_texture_pending) {
                dispatch_gns_resource(MAP_RESOURCE_TEXTURE, g_map_data_load_buffer);
            }
            if (g_map_palette_pending) {
                dispatch_gns_resource(MAP_RESOURCE_APPLY_PALETTES, (map_resource_t*)g_map_pending_palette_data);
            }
            g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
            break;
        /* Rebuild the mesh render records once in each render buffer: each
         * state waits until the current buffer is not the one last written. */
        case MAP_LOAD_STATE_MESH_RECORDS:
            if (s_last_render_buffer != g_render_current_buffer) {
                s_textured_triangle_count = 0;
                s_textured_quad_count = 0;
                s_untextured_triangle_count = 0;
                s_untextured_quad_count = 0;

                for (i = 0; i < 9; i++) {
                    if (s_map_mesh_part_geometry[i]) {
                        init_mesh_render_records(s_map_mesh_part_geometry[i]);
                    }
                    s_textured_triangle_count += g_map_mesh_parts[i].textured_triangle_count;
                    s_textured_quad_count += g_map_mesh_parts[i].textured_quad_count;
                    s_untextured_triangle_count += g_map_mesh_parts[i].untextured_triangle_count;
                    s_untextured_quad_count += g_map_mesh_parts[i].untextured_quad_count;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_MESH_RECORDS_SECOND;
            }
            break;
        case MAP_LOAD_STATE_MESH_RECORDS_SECOND:
            if (s_last_render_buffer != g_render_current_buffer) {
                s_textured_triangle_count = 0;
                s_textured_quad_count = 0;
                s_untextured_triangle_count = 0;
                s_untextured_quad_count = 0;

                for (i = 0; i < 9; i++) {
                    if (s_map_mesh_part_geometry[i]) {
                        init_mesh_render_records(s_map_mesh_part_geometry[i]);
                    }
                    s_map_mesh_part_geometry[i] = 0;
                    s_textured_triangle_count += g_map_mesh_parts[i].textured_triangle_count;
                    s_textured_quad_count += g_map_mesh_parts[i].textured_quad_count;
                    s_untextured_triangle_count += g_map_mesh_parts[i].untextured_triangle_count;
                    s_untextured_quad_count += g_map_mesh_parts[i].untextured_quad_count;
                }
                g_map_resource_load_state = MAP_LOAD_STATE_IDLE;
            }
            break;
        }
    }
    return g_map_gns_load_phase;
}

/* Selects the last primary-mesh record, then substitutes an alternate whose
 * layout/weather/time comparison matches, and loads the selected record's
 * file into a temporary 0x20000-byte buffer. The loader's fourth argument is
 * the resource type, as at its other call sites; start_file_load
 * does not read it. `palette_mode` is the animation mode the alternate mesh's
 * palette is applied with (map_palette_set_colors). Returns the load
 * state, nonzero until the palette has been applied.
 *
 * Retail bug: the second step applies the palette and frees the buffer while
 * the read is still in progress, and waits forever once it has finished. The
 * only caller runs while g_map_refresh_pending is set, which nothing sets. */
s32 map_mesh_load_palette(s32 palette_mode) {
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
                state = script_variable_get(g_map_gns_records[i].script_variable_id) & 0xfff;
                state |= (script_variable_get(SCRIPT_VAR_WEATHER) & 7) << 12;
                state |= (script_variable_get(SCRIPT_VAR_TIME_OF_DAY) & 1) << 15;
                if (gns_record_matches_state(
                        g_map_gns_records[i].map_state_comparison, g_map_gns_records[i].map_state, state)) {
                    g_map_selected_mesh_record = g_map_gns_records[i];
                }
            }
        }
        g_map_mesh_load_buffer = heap_alloc(0x20000);
        if (g_map_mesh_load_buffer == 0) {
            system_report_error(2, 0x3c9);
        }
        start_file_load(s_map_directory_path, &g_map_selected_mesh_record.load_info, g_map_mesh_load_buffer,
            MAP_RESOURCE_MESH_ALTERNATE);
        g_map_mesh_load_state = MAP_MESH_LOAD_WAITING;
        break;

    case MAP_MESH_LOAD_WAITING:
        if (file_get_load_state() != 0) {
            s32 mesh_data_offset;
            map_resource_t* mesh_data;

            mesh_data = g_map_mesh_load_buffer;
            /* Retail scales this byte offset as a 32-bit-element index. */
            mesh_data_offset = mesh_data->mesh_file[MAP_RESOURCE_PALETTES] * 4;
            mesh_data = (map_resource_t*)(mesh_data->bytes + mesh_data_offset);
            map_palette_set_colors(palette_mode, MAP_CLUT_ROW_MARKED_TILES, 0, 1, mesh_data->clut);
            heap_free(g_map_mesh_load_buffer);
            g_map_mesh_load_state = MAP_MESH_LOAD_IDLE;
        }
        break;
    }
    return g_map_mesh_load_state;
}

/*
 * Append one mesh part's polygons to the map's polygon tables: the vertex
 * positions of all four polygon classes, and the normals and terrain-tile
 * locations of the textured ones. `metadata` receives the part's first index
 * and count per class. Also asks map_update_load to rebuild the render
 * records over the next two frames.
 *
 * The geometry block starts with four polygon counts, then stores positions
 * for all four polygon classes, normals for textured polygons, per-polygon
 * render data, and finally terrain-tile locations for textured polygons.
 * Each position/normal vector occupies eight renderer bytes: XYZ followed by
 * a spare 16-bit value.  The first position vector's spare 16-bit value holds the
 * polygon's packed terrain-tile location.
 * Target 0x800f4dd4 is the geometry pass for primary and animated meshes;
 * init_mesh_render_records performs the separate packet
 * pass. Count order is textured triangles/quads, then untextured
 * triangles/quads. The mesh metadata keeps the starting indices and the
 * incoming counts; the persistent totals advance at the end.
 */
static void append_mesh_geometry(const map_mesh_geometry_t* geometry_data, map_mesh_part_t* metadata) {
    /* Named count loads fold the retail four 16-bit cursor advances. */
    const u16* source = &geometry_data->textured_triangle_count;
    s32 i;
    s32 textured_triangle_count;
    s32 textured_quad_count;
    s32 untextured_triangle_count;
    s32 untextured_quad_count;

    g_map_resource_load_state = MAP_LOAD_STATE_MESH_RECORDS;
    s_last_render_buffer = g_render_current_buffer;

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
 * Fill the current render buffer's packets for one mesh part, after the
 * records already filled this pass. Runtime polygon records interleave GPU
 * packet fields with renderer state.
 * This pass skips the geometry and normal vectors consumed by
 * append_mesh_geometry, then appends UV coordinates, CLUT commands,
 * texture-page commands, and the initial state of untextured records.
 * Target 0x800f5578 reads textured triangles of five 16-bit values and textured quads
 * of six 16-bit values. 16-bit CLUT values keep their low six bits (the CLUT) and gain the map CLUT row;
 * texture pages are copied verbatim. Destination strides are 0x28/0x34 for
 * textured triangles/quads and 0x14/0x18 for untextured triangles/quads.
 * Untextured POLY_F3/POLY_F4 packets start with color 0/0/1.
 */
static void init_mesh_render_records(const map_mesh_geometry_t* geometry_data) {
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

    s_last_render_buffer = g_render_current_buffer;

    textured_triangle_count = *source++;
    textured_quad_count = *source++;
    untextured_triangle_count = *source++;
    untextured_quad_count = *source++;

    triangle_vector_halfwords = textured_triangle_count * 9;
    quad_vector_halfwords = textured_quad_count * 12;
    source += triangle_vector_halfwords + quad_vector_halfwords + untextured_triangle_count * 9
        + untextured_quad_count * 12 + triangle_vector_halfwords + quad_vector_halfwords;

    for (polygon_index = s_textured_triangle_count; polygon_index < textured_triangle_count + s_textured_triangle_count;
        polygon_index++) {
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
        g_render_current_buffer->gt3[polygon_index].u0 = u0;
        g_render_current_buffer->gt3[polygon_index].u1 = u1;
        g_render_current_buffer->gt3[polygon_index].u2 = u2;
        g_render_current_buffer->gt3[polygon_index].v0 = v0;
        g_render_current_buffer->gt3[polygon_index].v1 = v1;
        g_render_current_buffer->gt3[polygon_index].v2 = v2;
        g_render_current_buffer->gt3[polygon_index].clut = (clut_data & 0x3f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        g_render_current_buffer->gt3[polygon_index].tpage = texture_page;
    }

    for (polygon_index = s_textured_quad_count; polygon_index < textured_quad_count + s_textured_quad_count;
        polygon_index++) {
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
        g_render_current_buffer->gt4[polygon_index].u0 = u0;
        g_render_current_buffer->gt4[polygon_index].u1 = u1;
        g_render_current_buffer->gt4[polygon_index].u2 = u2;
        g_render_current_buffer->gt4[polygon_index].u3 = u3;
        g_render_current_buffer->gt4[polygon_index].v0 = v0;
        g_render_current_buffer->gt4[polygon_index].v1 = v1;
        g_render_current_buffer->gt4[polygon_index].v2 = v2;
        g_render_current_buffer->gt4[polygon_index].v3 = v3;
        g_render_current_buffer->gt4[polygon_index].clut = (clut_data & 0x3f) | MAP_CLUT(MAP_CLUT_ROW_MAP, 0);
        g_render_current_buffer->gt4[polygon_index].tpage = texture_page;
    }

    for (polygon_index = s_untextured_triangle_count;
        polygon_index < untextured_triangle_count + s_untextured_triangle_count; polygon_index++) {
        g_render_current_buffer->f3[polygon_index].r0 = 0;
        g_render_current_buffer->f3[polygon_index].g0 = 0;
        g_render_current_buffer->f3[polygon_index].b0 = 1;
    }

    for (polygon_index = s_untextured_quad_count; polygon_index < untextured_quad_count + s_untextured_quad_count;
        polygon_index++) {
        g_render_current_buffer->f4[polygon_index].r0 = 0;
        g_render_current_buffer->f4[polygon_index].g0 = 0;
        g_render_current_buffer->f4[polygon_index].b0 = 1;
    }
}

/* weather sounds */

/* Stop the weather sound effects and every color rotation the 32 map texture
 * animations hold a handle for. Always returns 0. */
s32 map_stop_clut_cycles_and_weather_sfx(void) {
    s32 index;

    stop_weather_sfx();

    for (index = 0; index < MAP_TEXTURE_ANIMATION_COUNT; index++) {
        if (s_map_texture_animation_cycle_handles[index]) {
            map_palette_stop_clut_cycle(s_map_texture_animation_cycle_handles[index]);
            s_map_texture_animation_cycle_handles[index] = 0;
        }
    }

    g_weather_sfx_enabled = 0;
    return 0;
}

/* Enable the weather sounds and start rain when its sound ID is stored. */
void weather_start_sfx(void) {
    s32 sound_id = g_weather_rain_sfx_id;

    g_weather_sfx_enabled = 1;
    if (sound_id != 0) {
        sound_play_background(sound_id);
    }
}

/* Start map weather effect mode 0x96 for `value` frames, from frame 0; effect
 * E458 passes the frames left of its timeline. An id other than 0x96 does
 * nothing. g_weather_timed_transform_unread receives 0x95 and no code reads it. */
void weather_start_timed_transform(s32 id, s32 value) {
    if (id == WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM) {
        g_weather_timed_transform_unread = 0x95;
        g_weather_timed_transform_duration = value;
        s_weather_timed_transform_frame = 0;
        g_weather_effect_mode = WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM;
    }
}

/* Stop the rain sound and the two thunder sounds and forget their ids. The result
 * is an unset local; no caller reads it. */
static s32 stop_weather_sfx(void) {
    s32 result;
    s32 sound_id = g_weather_rain_sfx_id;

    if (sound_id != 0) {
        sound_stop_sfx(sound_id);
        g_weather_rain_sfx_id = 0;
    }

    sound_id = g_weather_thunder_a_sfx_id;
    if (sound_id != 0) {
        sound_stop_sfx(sound_id);
        g_weather_thunder_a_sfx_id = 0;
    }

    sound_id = g_weather_thunder_b_sfx_id;
    if (sound_id != 0) {
        sound_stop_sfx(sound_id);
        g_weather_thunder_b_sfx_id = 0;
    }

    /* The target returns an indeterminate saved-register value. */
    return result;
}

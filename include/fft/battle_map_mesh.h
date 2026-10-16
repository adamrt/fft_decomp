#ifndef BATTLE_MAP_MESH_H
#define BATTLE_MAP_MESH_H

/* BATTLE map mesh: polygon geometry, animated mesh parts, texture animations and the mesh renderer. */

#include "fft/battle_gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* geometry */
/* Serialized GNS primary/animated mesh geometry. Counts are unsigned; the
 * variable tail stores unpadded XYZ 16-bit values, normals, materials and terrain
 * coordinates. Native renderer position/normal records have different strides. */
typedef struct map_mesh_geometry {
    u16 textured_triangle_count;   /* 0x00 */
    u16 textured_quad_count;       /* 0x02 */
    u16 untextured_triangle_count; /* 0x04 */
    u16 untextured_quad_count;     /* 0x06 */
    u16 payload[0];                /* 0x08; count-dependent sections */
} map_mesh_geometry_t;
typedef char map_mesh_geometry_prefix_must_be_8[sizeof(map_mesh_geometry_t) == 8 ? 1 : -1];

/* Polygon position and normal records copied from the mesh geometry block.
 * Each XYZ vector occupies eight bytes. The first position's spare 16-bit
 * terrain tile has height level in bit 0, Z in bits 1-7 and X in bits 8-15;
 * 0xfffe marks a polygon without a terrain tile. */
enum { MAP_POLYGON_TILE_NONE = 0xfffe };

typedef struct map_mesh_triangle_positions {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
} map_mesh_triangle_positions_t;

typedef struct map_mesh_quad_positions {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
    s16 x3, y3, z3;
    u16 _unused_1e;
} map_mesh_quad_positions_t;

typedef struct map_mesh_triangle_normals {
    SVECTOR vertex[3];
} map_mesh_triangle_normals_t;

typedef struct map_mesh_quad_normals {
    SVECTOR vertex[4];
} map_mesh_quad_normals_t;

/* polygon_flags of the textured map polygons (MAP_RESOURCE_POLYGON_FLAGS).
 * The four map_queue_ routines skip a polygon whose hide bit for the
 * camera yaw is set. No mesh file sets bits 0, 1 or 14. */
enum {
    /* Working copy of LIT, the bit the queue routines test: the polygon's
     * colors come from its normals this frame. Cleared on a polygon a tile
     * mark tints. */
    MAP_POLYGON_FLAG_LIT_ACTIVE = 0x0001,
    /* Hidden while the yaw is in 45-degree band n: bit 2 + n, where
     * n = ((yaw - 1) & 0xe00) >> 9. Tested every frame. */
    MAP_POLYGON_FLAG_HIDDEN_IN_YAW_BAND_0 = 0x0004,
    /* Hidden while the camera rests on diagonal n: bit 10 + n, where
     * n = yaw >> 10. Tested only when (yaw + 0x200) & 0x3ff is 0. */
    MAP_POLYGON_FLAG_HIDDEN_AT_RESTING_YAW_0 = 0x0400,
    MAP_POLYGON_FLAG_HIDDEN_MASK = 0x3ffc,
    /* Lit from its normals each frame; a polygon without it keeps the flat
     * ambient color. */
    MAP_POLYGON_FLAG_LIT = 0x8000,
};

/* Commands of battle_map_polygon_flag_command. */
typedef enum map_polygon_flag_command {
    MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE = 0x46,    /* copy LIT into LIT_ACTIVE on every polygon */
    MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE = 0x47,    /* clear LIT_ACTIVE everywhere */
    MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT = 0x94, /* repaint the polygons without LIT in the ambient color */
} map_polygon_flag_command_e;

/* mesh parts */
/* 0x98-byte map mesh part records of g_battle_map_mesh_parts. Each of the rotation,
 * translation and scale channels has its current value, the start of the
 * running keyframe and its target or step. */
typedef struct map_mesh_part {
    SVECTOR rotation_start;    /* 0x00 */
    SVECTOR rotation;          /* 0x08 */
    SVECTOR rotation_step;     /* 0x10 */
    SVECTOR translation_start; /* 0x18 */
    SVECTOR translation;       /* 0x20 */
    SVECTOR translation_step;  /* 0x28 */
    VECTOR scale_start;        /* 0x30; the scales reset to {0x1000, 0x1000, 0x1000} */
    VECTOR scale;              /* 0x40 */
    VECTOR scale_step;         /* 0x50 */
    MATRIX matrix;             /* 0x60 */
    s16 elapsed;               /* 0x80; time in the keyframe */
    s16 duration;              /* 0x82; tween length */
    u8 speed;                  /* 0x84; 4.4, 0x10 = 1 */
    u8 next_instruction;       /* 0x85 */
    u8 parent;                 /* 0x86; parent part, from mesh properties byte 0 */
    u8 keyframe;               /* 0x87; 0 = fetch next */
    /* 0x88: per-part start indices and counts of its render records, written
     * by battle_map_append_mesh_geometry. */
    u16 textured_triangle_start;
    u16 textured_quad_start;
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    u16 textured_triangle_count; /* 0x90 */
    u16 textured_quad_count;
    u16 untextured_triangle_count;
    u16 untextured_quad_count;
} map_mesh_part_t;
typedef char map_mesh_part_size_must_be_152[(sizeof(map_mesh_part_t) == 152) ? 1 : -1];

/* Channels of a mesh keyframe's per-channel arrays (angle_start, angle_end,
 * tween_flags) and of a part's animation state. */
typedef enum map_mesh_channel {
    MAP_MESH_CHANNEL_ROTATION_X = 0,
    MAP_MESH_CHANNEL_ROTATION_Y = 1,
    MAP_MESH_CHANNEL_ROTATION_Z = 2,
    MAP_MESH_CHANNEL_TRANSLATION_X = 3,
    MAP_MESH_CHANNEL_TRANSLATION_Y = 4,
    MAP_MESH_CHANNEL_TRANSLATION_Z = 5,
    MAP_MESH_CHANNEL_SCALE_X = 6,
    MAP_MESH_CHANNEL_SCALE_Y = 7,
    MAP_MESH_CHANNEL_SCALE_Z = 8,
    MAP_MESH_CHANNEL_COUNT = 9,
} map_mesh_channel_e;

/* One 0x50-byte keyframe from a mesh animation record of type 1.
 * Keyframe ids are 1-based; entry 0 is never loaded. Each of the nine channels
 * (rotation, translation and scale of a mesh part) has a target value, a
 * start/end angle for the sine modes and a 16-bit mode flags whose bits 0 and 1
 * are TweenTo and TweenBy. */
typedef struct map_mesh_keyframe {
    SVECTOR rotation;                        /* 0x00; target */
    SVECTOR translation;                     /* 0x08; target */
    s16 scale_x;                             /* 0x10; ONE = 1.0 */
    s16 scale_y;                             /* 0x12 */
    s16 scale_z;                             /* 0x14 */
    s16 _unused_16;                          /* 0x16 */
    s16 angle_start[MAP_MESH_CHANNEL_COUNT]; /* 0x18 */
    s16 angle_end[MAP_MESH_CHANNEL_COUNT];   /* 0x2a */
    u16 tween_flags[MAP_MESH_CHANNEL_COUNT]; /* 0x3c */
    u16 _unused_4e;                          /* 0x4e */
} map_mesh_keyframe_t;
typedef char map_mesh_keyframe_size_must_be_80[(sizeof(map_mesh_keyframe_t) == 80) ? 1 : -1];

/* map_mesh_keyframe_t.tween_flags: how one channel reaches its keyframe value. */
typedef enum map_mesh_tween_flags {
    MAP_MESH_TWEEN_TO = 0x01, /* delta = target - start */
    MAP_MESH_TWEEN_BY = 0x02, /* delta = the keyframe value */
    MAP_MESH_TWEEN_LINEAR = 0x04,
    MAP_MESH_TWEEN_SINE = 0x08,        /* delta * sin */
    MAP_MESH_TWEEN_RAISED_SINE = 0x10, /* delta * (sin + 1) */
} map_mesh_tween_flags_e;

/* One instruction from a mesh animation record of type 2. A duration of
 * 0 stops the part's animation when it is reached. */
typedef struct map_mesh_instruction {
    u8 keyframe;  /* 1-based keyframe id */
    u8 next;      /* index of the following instruction */
    u16 duration; /* frames */
} map_mesh_instruction_t;

/* A mesh part's 8 animation states of 17 instructions (index 0 unused).
 * States are loaded state-major; row 0 is also the running copy that event
 * command 0x80 overwrites with the selected state. */
typedef struct map_mesh_instructions {
    map_mesh_instruction_t states[8][17];
} map_mesh_instructions_t;
typedef char map_mesh_instructions_size_must_be_544[(sizeof(map_mesh_instructions_t) == 544) ? 1 : -1];

extern map_mesh_part_t g_battle_map_mesh_parts[9];
extern map_mesh_keyframe_t g_battle_map_mesh_animation_keyframes[];
extern map_mesh_instructions_t g_battle_map_mesh_animation_instructions[9];
extern s32 g_battle_map_mesh_part_animation_states[];
extern const map_mesh_geometry_t* g_battle_map_mesh_part_geometry[9];

/* texture animations */
/* map_texture_animation_t.mode: the technique of a map texture animation row and, for the
 * values from 128, its playback state. */
typedef enum map_texture_animation_mode {
    MAP_TEXTURE_ANIMATION_MODE_NONE = 0, /* an unused row */
    MAP_TEXTURE_ANIMATION_MODE_UV_LOOP = 1,
    MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG = 2,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP = 3,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG = 4,
    /* 5-28 play once when a map data command starts them; the eight ids of a group behave alike */
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0 = 5,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_1 = 6,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_2 = 7,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_3 = 8,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_4 = 9,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_5 = 10,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_6 = 11,
    MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_7 = 12,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_0 = 13,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_1 = 14,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_2 = 15,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_3 = 16,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_4 = 17,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_5 = 18,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_6 = 19,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_7 = 20,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0 = 21,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_1 = 22,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_2 = 23,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_3 = 24,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_4 = 25,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_5 = 26,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_6 = 27,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_7 = 28,
    /* starts a battle_map_start_clut_cycle over colors canvas_x .. canvas_x + width - 1 of MAP_CLUT_ROW_MAP */
    MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE = 29,
    /* Playback states written and dispatched by battle_map_update_texture_animations. */
    MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK = 128,
    MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK = 129,
    MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING = 130,
} map_texture_animation_mode_e;

enum { MAP_TEXTURE_ANIMATION_COUNT = 32 };

/* Runtime copy of one 0x14-byte map texture animation instruction
 * (MAP_RESOURCE_TEXTURE_ANIMATIONS, 32 rows).
 * UV animations move a canvas-sized frame through VRAM with the DR_MOVE
 * packet of the same index; palette animations reload CLUT
 * canvas_x / 16 of MAP_CLUT_ROW_MAP from the palette frames at g_battle_map_palette_animation_frames. */
typedef struct map_texture_animation {
    s16 canvas_x;   /* 0x00 */
    s16 canvas_y;   /* 0x02 */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 frame_x;    /* 0x08 */
    s16 frame_y;    /* 0x0a */
    u8 column;      /* 0x0c */
    u8 row;         /* 0x0d */
    u8 mode;        /* 0x0e; map_texture_animation_mode_e */
    u8 frame_count; /* 0x0f */
    u8 frame;       /* 0x10 */
    u8 duration;    /* 0x11 */
    u8 timer;       /* 0x12 */
    u8 active;      /* 0x13 */
} map_texture_animation_t;
typedef char map_texture_animation_size_must_be_20[(sizeof(map_texture_animation_t) == 20) ? 1 : -1];

extern s32 g_battle_map_texture_animation_cycle_handles[MAP_TEXTURE_ANIMATION_COUNT];
extern map_texture_animation_t g_battle_map_texture_animations[MAP_TEXTURE_ANIMATION_COUNT];

void battle_map_freeze(void);
void battle_map_unfreeze(void);

/* rendering */
/* Commands of battle_map_configure_effect_data_state: the ordering-table
 * handling of untextured map polygons while an effect draws. */
typedef enum map_effect_data_command {
    MAP_EFFECT_DATA_END = 0x87,
    MAP_EFFECT_DATA_BEGIN = 0x88,
} map_effect_data_command_e;

extern s32 g_battle_map_textured_quad_count;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_tilt_target;
extern s32 g_battle_map_untextured_quad_count;
extern s32 g_battle_map_untextured_triangle_count;
extern s32 g_battle_map_zoom_target;
extern VECTOR g_battle_map_camera_zoom;

/* Untextured map polygons go to OT slot (otz & mask) | fixed index: {0, 4}
 * normally, {-1, 0} while map effect data is active (command 0x88), so a
 * non-zero mask also marks that state. */
extern s32 g_battle_map_untextured_fixed_ot_index;
extern s32 g_battle_map_untextured_otz_mask;
extern CVECTOR g_battle_map_white_cvector;

/* Default vectors shared with battle_map_apply_gns_command, which emits them. */
extern SVECTOR g_battle_map_zero_svector;
extern VECTOR g_battle_map_unit_scale_vector;
extern SVECTOR g_battle_map_camera_rotation;
extern map_mesh_triangle_positions_t* g_battle_map_part_textured_triangle_positions;
extern map_mesh_quad_positions_t* g_battle_map_part_textured_quad_positions;
extern map_mesh_triangle_positions_t* g_battle_map_part_untextured_triangle_positions;
extern map_mesh_quad_positions_t* g_battle_map_part_untextured_quad_positions;
extern map_mesh_triangle_normals_t* g_battle_map_part_textured_triangle_normals;
extern map_mesh_quad_normals_t* g_battle_map_part_textured_quad_normals;
extern s32 g_battle_map_part_textured_quad_count;
extern s32 g_battle_map_part_textured_triangle_count;
extern s32 g_battle_map_part_untextured_quad_count;
extern s32 g_battle_map_part_untextured_triangle_count;
extern map_mesh_triangle_positions_t g_battle_map_textured_triangle_positions[BATTLE_RENDER_TEXTURED_TRIANGLE_COUNT];
extern map_mesh_quad_positions_t g_battle_map_textured_quad_positions[BATTLE_RENDER_TEXTURED_QUAD_COUNT];
extern map_mesh_triangle_positions_t
    g_battle_map_untextured_triangle_positions[BATTLE_RENDER_UNTEXTURED_TRIANGLE_COUNT];
extern map_mesh_quad_positions_t g_battle_map_untextured_quad_positions[BATTLE_RENDER_UNTEXTURED_QUAD_COUNT];
extern map_mesh_triangle_normals_t g_battle_map_textured_triangle_normals[];
extern map_mesh_quad_normals_t g_battle_map_textured_quad_normals[];
extern u8 g_battle_map_polygon_flag_clear_countdown;
extern s32 g_battle_map_primary_textured_quad_count;
extern s32 g_battle_map_primary_textured_triangle_count;
extern s32 g_battle_map_primary_untextured_quad_count;
extern s32 g_battle_map_primary_untextured_triangle_count;

void battle_map_append_mesh_geometry(const map_mesh_geometry_t* geometry_data, map_mesh_part_t* metadata);
void map_queue_textured_quads(u32* otag, POLY_GT4* prims, s32 yaw, s32* unused_count);
void map_queue_textured_triangles(u32* otag, POLY_GT3* prims, s32 yaw, s32* unused_count);
void map_queue_untextured_quads(u32* otag, POLY_F4* prims, s32 yaw, s32* unused_count);
void map_queue_untextured_triangles(u32* otag, POLY_F3* prims, s32 yaw, s32* unused_count);
s32 battle_map_configure_effect_data_state(map_effect_data_command_e command);
void battle_map_reset_mesh_render_state(void);
void battle_map_draw_mesh_and_weather(MATRIX* camera_matrix);

#endif

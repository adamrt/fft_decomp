#ifndef BATTLE_MAP_MESH_H
#define BATTLE_MAP_MESH_H

/* Battle map mesh: polygon geometry and flags, animated mesh parts, texture animations, and the render records and
 * renderer that draw the map. */

#include "fft/render.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct battle_map_state_record;

/* geometry */
/* Serialized GNS primary/animated mesh geometry. Counts are unsigned; the
 * variable tail stores unpadded XYZ 16-bit values, normals, materials and terrain
 * coordinates. Native renderer position/normal records have different strides. */
typedef struct battle_map_mesh_geometry {
    u16 textured_triangle_count;   /* 0x00 */
    u16 textured_quad_count;       /* 0x02 */
    u16 untextured_triangle_count; /* 0x04 */
    u16 untextured_quad_count;     /* 0x06 */
    u16 payload[0];                /* 0x08; count-dependent sections */
} battle_map_mesh_geometry_t;
typedef char battle_map_mesh_geometry_size_must_be_8[sizeof(battle_map_mesh_geometry_t) == 8 ? 1 : -1];

/* Polygon position and normal records copied from the mesh geometry block.
 * Each XYZ vector occupies eight bytes. The first position's spare 16-bit
 * terrain tile has height level in bit 0, Z in bits 1-7 and X in bits 8-15;
 * 0xfffe marks a polygon without a terrain tile. */
enum { BATTLE_MAP_POLYGON_TILE_NONE = 0xfffe };

typedef struct battle_map_mesh_triangle_positions {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
} battle_map_mesh_triangle_positions_t;

typedef struct battle_map_mesh_quad_positions {
    s16 x0, y0, z0;
    u16 terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _unused_16;
    s16 x3, y3, z3;
    u16 _unused_1e;
} battle_map_mesh_quad_positions_t;

typedef struct battle_map_mesh_triangle_normals {
    SVECTOR vertex[3];
} battle_map_mesh_triangle_normals_t;

typedef struct battle_map_mesh_quad_normals {
    SVECTOR vertex[4];
} battle_map_mesh_quad_normals_t;

/* polygon_flags of the textured map polygons (BATTLE_MAP_RESOURCE_POLYGON_FLAGS).
 * The four battle_map_queue_ routines skip a polygon whose hide bit for the
 * camera yaw is set. No mesh file sets bits 0, 1 or 14. */
enum {
    /* Working copy of LIT, the bit the queue routines test: the polygon's
     * colors come from its normals this frame. Cleared on a polygon a tile
     * mark tints. */
    BATTLE_MAP_POLYGON_FLAG_LIT_ACTIVE = 0x0001,
    /* Hidden while the yaw is in 45-degree band n: bit 2 + n, where
     * n = ((yaw - 1) & 0xe00) >> 9. Tested every frame. */
    BATTLE_MAP_POLYGON_FLAG_HIDDEN_IN_YAW_BAND_0 = 0x0004,
    /* Hidden while the camera rests on diagonal n: bit 10 + n, where
     * n = yaw >> 10. Tested only when (yaw + 0x200) & 0x3ff is 0. */
    BATTLE_MAP_POLYGON_FLAG_HIDDEN_AT_RESTING_YAW_0 = 0x0400,
    BATTLE_MAP_POLYGON_FLAG_HIDDEN_MASK = 0x3ffc,
    /* Lit from its normals each frame; a polygon without it keeps the flat
     * ambient color. */
    BATTLE_MAP_POLYGON_FLAG_LIT = 0x8000
};

/* Commands of polygon_flags_dispatch. */
typedef enum battle_map_polygon_flag_command {
    BATTLE_MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE = 0x46,   /* copy LIT into LIT_ACTIVE on every polygon */
    BATTLE_MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE = 0x47,   /* clear LIT_ACTIVE everywhere */
    BATTLE_MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT = 0x94 /* repaint the polygons without LIT in the ambient color */
} battle_map_polygon_flag_command_e;

/* mesh parts */
/* 0x98-byte map mesh part records of g_battle_map_mesh_parts. Each of the rotation,
 * translation and scale channels has its current value, the start of the
 * running keyframe and its target or step. */
typedef struct battle_map_mesh_part {
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
     * by battle_map_mesh_append_geometry. */
    u16 textured_triangle_start;
    u16 textured_quad_start;
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    u16 textured_triangle_count; /* 0x90 */
    u16 textured_quad_count;
    u16 untextured_triangle_count;
    u16 untextured_quad_count;
} battle_map_mesh_part_t;
typedef char battle_map_mesh_part_size_must_be_152[(sizeof(battle_map_mesh_part_t) == 152) ? 1 : -1];

/* Channels of a mesh keyframe's per-channel arrays (angle_start, angle_end,
 * tween_flags) and of a part's animation state. */
typedef enum battle_map_mesh_channel {
    BATTLE_MAP_MESH_CHANNEL_ROTATION_X = 0,
    BATTLE_MAP_MESH_CHANNEL_ROTATION_Y = 1,
    BATTLE_MAP_MESH_CHANNEL_ROTATION_Z = 2,
    BATTLE_MAP_MESH_CHANNEL_TRANSLATION_X = 3,
    BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Y = 4,
    BATTLE_MAP_MESH_CHANNEL_TRANSLATION_Z = 5,
    BATTLE_MAP_MESH_CHANNEL_SCALE_X = 6,
    BATTLE_MAP_MESH_CHANNEL_SCALE_Y = 7,
    BATTLE_MAP_MESH_CHANNEL_SCALE_Z = 8,
    BATTLE_MAP_MESH_CHANNEL_COUNT = 9
} battle_map_mesh_channel_e;

/* One 0x50-byte keyframe from a mesh animation record of type 1.
 * Keyframe ids are 1-based; entry 0 is never loaded. Each of the nine channels
 * (rotation, translation and scale of a mesh part) has a target value, a
 * start/end angle for the sine modes and a 16-bit mode flags whose bits 0 and 1
 * are TweenTo and TweenBy. */
typedef struct battle_map_mesh_keyframe {
    SVECTOR rotation;                               /* 0x00; target */
    SVECTOR translation;                            /* 0x08; target */
    SVECTOR scale;                                  /* 0x10; ONE = 1.0; fourth halfword unused */
    s16 angle_start[BATTLE_MAP_MESH_CHANNEL_COUNT]; /* 0x18 */
    s16 angle_end[BATTLE_MAP_MESH_CHANNEL_COUNT];   /* 0x2a */
    u16 tween_flags[BATTLE_MAP_MESH_CHANNEL_COUNT]; /* 0x3c */
    u16 _unused_4e;                                 /* 0x4e */
} battle_map_mesh_keyframe_t;
typedef char battle_map_mesh_keyframe_size_must_be_80[(sizeof(battle_map_mesh_keyframe_t) == 80) ? 1 : -1];

/* battle_map_mesh_keyframe_t.tween_flags: how one channel reaches its keyframe value. */
typedef enum battle_map_mesh_tween_flags {
    BATTLE_MAP_MESH_TWEEN_TO = 0x01, /* delta = target - start */
    BATTLE_MAP_MESH_TWEEN_BY = 0x02, /* delta = the keyframe value */
    BATTLE_MAP_MESH_TWEEN_LINEAR = 0x04,
    BATTLE_MAP_MESH_TWEEN_SINE = 0x08,       /* delta * sin */
    BATTLE_MAP_MESH_TWEEN_RAISED_SINE = 0x10 /* delta * (sin + 1) */
} battle_map_mesh_tween_flags_e;

extern battle_map_mesh_part_t g_battle_map_mesh_parts[9];
extern const battle_map_mesh_geometry_t* g_battle_map_mesh_part_geometry[9];

/* texture animations */
/* battle_map_texture_animation_t.mode: the technique of a map texture animation row and, for the
 * values from 128, its playback state. */
typedef enum battle_map_texture_animation_mode {
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_NONE = 0, /* an unused row */
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_LOOP = 1,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG = 2,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP = 3,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG = 4,
    /* 5-28 play once when a map data command starts them; the eight ids of a group behave alike */
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0 = 5,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_1 = 6,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_2 = 7,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_3 = 8,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_4 = 9,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_5 = 10,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_6 = 11,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_7 = 12,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_0 = 13,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_1 = 14,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_2 = 15,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_3 = 16,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_4 = 17,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_5 = 18,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_6 = 19,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_7 = 20,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0 = 21,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_1 = 22,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_2 = 23,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_3 = 24,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_4 = 25,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_5 = 26,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_6 = 27,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_7 = 28,
    /* starts a battle_map_clut_start_cycle over colors canvas_x .. canvas_x + width - 1 of BATTLE_MAP_CLUT_ROW_MAP */
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE = 29,
    /* Playback states written and dispatched by texture_animations_update. */
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK = 128,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK = 129,
    BATTLE_MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING = 130
} battle_map_texture_animation_mode_e;

enum { BATTLE_MAP_TEXTURE_ANIMATION_COUNT = 32 };

/* Runtime copy of one 0x14-byte map texture animation instruction
 * (BATTLE_MAP_RESOURCE_TEXTURE_ANIMATIONS, 32 rows).
 * UV animations move a canvas-sized frame through VRAM with the DR_MOVE
 * packet of the same index; palette animations reload CLUT
 * canvas_x / 16 of BATTLE_MAP_CLUT_ROW_MAP from the palette frames at g_battle_map_palette_animation_frames. */
typedef struct battle_map_texture_animation {
    s16 canvas_x;   /* 0x00 */
    s16 canvas_y;   /* 0x02 */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 frame_x;    /* 0x08 */
    s16 frame_y;    /* 0x0a */
    u8 column;      /* 0x0c */
    u8 row;         /* 0x0d */
    u8 mode;        /* 0x0e; battle_map_texture_animation_mode_e */
    u8 frame_count; /* 0x0f */
    u8 frame;       /* 0x10 */
    u8 duration;    /* 0x11 */
    u8 timer;       /* 0x12 */
    u8 active;      /* 0x13 */
} battle_map_texture_animation_t;
typedef char battle_map_texture_animation_size_must_be_20[(sizeof(battle_map_texture_animation_t) == 20) ? 1 : -1];

extern s32 g_battle_map_texture_animation_cycle_handles[BATTLE_MAP_TEXTURE_ANIMATION_COUNT];

void battle_map_freeze(void);
void battle_map_unfreeze(void);

/* render records */
extern battle_map_mesh_triangle_positions_t g_battle_map_textured_triangle_positions[RENDER_TEXTURED_TRIANGLE_COUNT];
extern battle_map_mesh_quad_positions_t g_battle_map_textured_quad_positions[RENDER_TEXTURED_QUAD_COUNT];
extern battle_map_mesh_triangle_positions_t
    g_battle_map_untextured_triangle_positions[RENDER_UNTEXTURED_TRIANGLE_COUNT];
extern battle_map_mesh_quad_positions_t g_battle_map_untextured_quad_positions[RENDER_UNTEXTURED_QUAD_COUNT];
extern battle_map_mesh_triangle_normals_t g_battle_map_textured_triangle_normals[];
extern battle_map_mesh_quad_normals_t g_battle_map_textured_quad_normals[];
extern s32 g_battle_map_textured_quad_count;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_untextured_quad_count;
extern s32 g_battle_map_untextured_triangle_count;

void battle_map_mesh_append_geometry(const battle_map_mesh_geometry_t* geometry_data, battle_map_mesh_part_t* metadata);

/* rendering */
/* Commands of battle_map_effect_data_state_set: the ordering-table
 * handling of untextured map polygons while an effect draws. */
typedef enum battle_map_effect_data_command {
    BATTLE_MAP_EFFECT_DATA_END = 0x87,
    BATTLE_MAP_EFFECT_DATA_BEGIN = 0x88
} battle_map_effect_data_command_e;

void battle_map_mesh_reset_render_state(void);
void battle_map_mesh_weather_draw(MATRIX* camera_matrix);

/* The four primitive queues have no C source: hand-written MIPS in the original; the GTE routines are kept as original
 * bytes through the `regions:` rows of target/battle.yaml (each returns from its prologue and keeps its body after the
 * jr ra); a port supplies the bodies. */
void battle_map_queue_textured_quads(u32* otag, POLY_GT4* prims, s32 yaw, s32* unused_count);
void battle_map_queue_textured_triangles(u32* otag, POLY_GT3* prims, s32 yaw, s32* unused_count);
void battle_map_queue_untextured_quads(u32* otag, POLY_F4* prims, s32 yaw, s32* unused_count);
void battle_map_queue_untextured_triangles(u32* otag, POLY_F3* prims, s32 yaw, s32* unused_count);
s32 battle_map_effect_data_state_set(battle_map_effect_data_command_e command);

#endif

#ifndef BATTLE_MAP_RESOURCE_H
#define BATTLE_MAP_RESOURCE_H

/* BATTLE map loading: MAPnnn.GNS records, mesh-file resources and the resource loader. */

#include "fft/battle_map_mesh.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* GNS records */
typedef enum gns_map_state_comparison {
    GNS_MAP_STATE_EQUAL = 1,
    GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD = 2,
    GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD = 3,
} gns_map_state_comparison_e;

/* GNS command rows follow the file-resource rows and use the same tag byte. */
typedef enum gns_map_command_type {
    GNS_COMMAND_SET_CAMERA_POSITION = 0x83,
    GNS_COMMAND_SET_CAMERA_ROTATION = 0x84,
    GNS_COMMAND_SET_INDOOR = 0x85,
    GNS_COMMAND_SET_OUTDOOR = 0x86,
    GNS_COMMAND_UNHANDLED_87 = 0x87, /* rows of this type are in 14 maps' files; the dispatcher has no case for it */
    GNS_COMMAND_CLEAR_SNOW = 0x88,
    GNS_COMMAND_SET_SNOW = 0x89,
    GNS_COMMAND_UNHANDLED_8A = 0x8a, /* 28-byte rows; forwarded unconditionally and ignored by the dispatcher */
    GNS_COMMAND_SET_WEATHER_MODIFIER = 0x8b,
} gns_map_command_type_e;

/* Structural bounds and the tag separating file rows from command records. */
enum {
    GNS_RECORD_NONE = 0,           /* empty file slot; also ends the command records */
    GNS_RESOURCE_TABLE_COUNT = 49, /* mesh 32-bit offsets and pending/loaded resource slots */
    GNS_RECORD_COMMAND_SEPARATOR = 0x80,
};

/* File-load payload at +0x06 of a GNS row. The loader compares all fourteen
 * bytes, including the fixed markers, before deciding whether to reload.
 * 32-bit fields remain byte arrays so enclosing rows retain 16-bit alignment. */
typedef union gns_load_info {
    struct {
        u16 _unused_00;         /* 0x3333 in file rows */
        u8 disc_lba_le[4];      /* row +0x08; absolute LBA, including its high 16 bits */
        u8 byte_length_le[4];   /* row +0x0c; sector-rounded byte count */
        u8 _unused_0a[3];       /* 55 66 77 in file rows */
        u8 file_present_marker; /* 0x88 in file rows; zero means no file */
    } fields;
    u8 bytes[14];
} gns_load_info_t;
typedef char gns_load_info_size_must_be_14[sizeof(gns_load_info_t) == 14 ? 1 : -1];

/*
 * A 20-byte row from the file section of MAPnnn.GNS, before the 0x80
 * separator. Resource type 0x31 marks repeated filler rows. For mesh records,
 * the loader compares map_state against a request assembled from the low 12
 * bits of the script variable at offset 0, weather in bits 12-14, and
 * time-of-day in bit 15.
 *
 * The two bytes at 4 also form the conventional little-endian GNS record type
 * (for example, 0x2e01 for a primary mesh).  The game reads them separately:
 * the low byte selects the state comparison and the high byte selects the
 * resource type. In all 2,987 pre-separator rows of the USA disc, +0x06 is
 * 0x3333 and +0x10..+0x13 are 55 66 77 88. The loader compares those bytes
 * when checking whether a file changed, and tests +0x13 for a nonzero marker.
 */
/* Record copies use lwl/lwr pairs; keep the record aligned to 16 bits. */
typedef struct gns_record {
    s16 script_variable_id;
    s16 map_state;
    u8 map_state_comparison; /* 0x04; gns_map_state_comparison_e */
    u8 resource_type;        /* 0x05; file resource or command type, or GNS_RECORD_COMMAND_SEPARATOR */
    gns_load_info_t load_info;
} gns_record_t;
typedef char gns_record_size_must_be_20[(sizeof(gns_record_t) == 20) ? 1 : -1];

/* Command rows after the 0x80 separator reuse the 20-byte prefix, but +0x08
 * is a byte stride (0x14 or 0x1c), not a disc LBA. The 0x1c rows have eight
 * more payload bytes after this prefix. +0x06/+0x09..+0x0b are zero in the
 * USA disc's command rows. */
typedef struct gns_command_record_prefix {
    u16 script_variable_id;  /* 0x00 */
    s16 map_state;           /* 0x02 */
    u8 map_state_comparison; /* 0x04; gns_map_state_comparison_e */
    u8 resource_type;        /* 0x05; gns_map_command_type_e, or GNS_RECORD_COMMAND_SEPARATOR */
    u16 _unused_06;
    u8 record_byte_length; /* 0x08 */
    u8 _unused_09;
    u16 _unused_0a;
    SVECTOR payload; /* 0x0c; followed by more bytes for 0x1c rows */
} gns_command_record_prefix_t;
typedef char gns_command_record_prefix_size_must_be_20[(sizeof(gns_command_record_prefix_t) == 20) ? 1 : -1];

/* mesh file resources */
/* Map resource IDs used by both GNS file rows and mesh chunk dispatch.
 * Mesh chunk IDs index a 49-entry table of 32-bit values, so each entry's
 * header byte position is ID * sizeof(u32): LIGHTING (0x19) is at 0x64,
 * and PALETTES (0x11) is at 0x44. Each value is a byte offset from the
 * beginning of the mesh file. */
typedef enum map_resource_type {
    /* Primary mesh data. */
    MAP_RESOURCE_GEOMETRY_PRIMARY = 0x10,
    MAP_RESOURCE_PALETTES = 0x11,       /* kept pending until MAP_RESOURCE_APPLY_PALETTES */
    MAP_RESOURCE_UNSUPPORTED_12 = 0x12, /* raises exception 0x3ca */

    /* Texture chunks and the complete external texture file. */
    MAP_RESOURCE_TEXTURE_QUARTER_0 = 0x13, /* 64x256 texture strip at VRAM x 0x300 */
    MAP_RESOURCE_TEXTURE_QUARTER_1 = 0x14,
    MAP_RESOURCE_TEXTURE_QUARTER_2 = 0x15,
    MAP_RESOURCE_TEXTURE_QUARTER_3 = 0x16,
    MAP_RESOURCE_TEXTURE = 0x17,

    /* Lighting, terrain, animations and map state. */
    MAP_RESOURCE_LIGHTING = 0x19, /* kept pending until MAP_RESOURCE_APPLY_LIGHTING */
    MAP_RESOURCE_TERRAIN = 0x1a,
    MAP_RESOURCE_TEXTURE_ANIMATIONS = 0x1b,
    MAP_RESOURCE_PALETTE_ANIMATION_FRAMES = 0x1c,
    MAP_RESOURCE_MARKED_TILE_PALETTES = 0x1f, /* the sixteen CLUTs of MAP_CLUT_ROW_MARKED_TILES */
    MAP_RESOURCE_STATE_RECORDS = 0x23,        /* map_state_record_t list */

    /* Additional mesh parts and polygon flags. */
    MAP_RESOURCE_GEOMETRY_PART_1 = 0x24, /* geometry of mesh parts 1-8 */
    MAP_RESOURCE_GEOMETRY_PART_2 = 0x25,
    MAP_RESOURCE_GEOMETRY_PART_3 = 0x26,
    MAP_RESOURCE_GEOMETRY_PART_4 = 0x27,
    MAP_RESOURCE_GEOMETRY_PART_5 = 0x28,
    MAP_RESOURCE_GEOMETRY_PART_6 = 0x29,
    MAP_RESOURCE_GEOMETRY_PART_7 = 0x2a,
    MAP_RESOURCE_GEOMETRY_PART_8 = 0x2b,
    MAP_RESOURCE_POLYGON_FLAGS = 0x2c,

    /* External mesh files and repeated file-section rows. */
    MAP_RESOURCE_MESH_PRIMARY = 0x2e,
    MAP_RESOURCE_MESH_OVERRIDE = 0x2f,
    MAP_RESOURCE_MESH_ALTERNATE = 0x30,
    MAP_RESOURCE_FILLER = 0x31, /* repeated file-section rows; the loader still visits them */
} map_resource_type_e;

/* Private dispatcher operations issued after a texture load, not serialized IDs. */
typedef enum map_resource_apply_command {
    MAP_RESOURCE_APPLY_LIGHTING = 0x32,
    MAP_RESOURCE_APPLY_PALETTES = 0x33,
} map_resource_apply_command_e;

/* Mesh files begin with 49 byte offsets indexed by map_resource_type_e.
 * The dispatcher visits IDs 0x10..0x30; zero means the chunk is absent.
 * Reserved offsets are zero on the USA disc. MAP000.5 alone has an embedded
 * texture quarter at resource ID 0x13. */
typedef u32 map_mesh_file_header_t[GNS_RESOURCE_TABLE_COUNT];
typedef char map_mesh_file_header_size_must_be_196[(sizeof(map_mesh_file_header_t) == 196) ? 1 : -1];

/* Terrain chunk: two dimensions followed by two fixed 256-tile layers. */
typedef struct map_terrain_resource {
    u8 width;                   /* 0x0000; map x dimension */
    u8 depth;                   /* 0x0001; map y dimension */
    map_tile_t tiles[2][0x100]; /* 0x0002; tile levels 0 and 1 */
} map_terrain_resource_t;
typedef char map_terrain_resource_size_must_be_4098[sizeof(map_terrain_resource_t) == 4098 ? 1 : -1];

/* Packed lighting: two matrices of nine 16-bit entries followed by three RGB colors. */
typedef struct map_light_resource {
    s16 color[3][3];         /* 0x00 */
    s16 direction[3][3];     /* 0x12 */
    rgb24_t ambient;         /* 0x24 */
    rgb24_t gradient_top;    /* 0x27 */
    rgb24_t gradient_bottom; /* 0x2a */
} map_light_resource_t;

/* Polygon-render properties: the 0x380-byte prefix is not consumed here. */
typedef struct map_polygon_flags_resource {
    u8 _unused_0000[0x380];
    u16 textured_triangles[512];  /* 0x0380 */
    u16 textured_quads[768];      /* 0x0780 */
    u16 untextured_triangles[64]; /* 0x0d80 */
    u16 untextured_quads[256];    /* 0x0e00 */
} map_polygon_flags_resource_t;
typedef char map_polygon_flags_resource_size_must_be_4096[sizeof(map_polygon_flags_resource_t) == 4096 ? 1 : -1];

/* Nested records in MAP_RESOURCE_STATE_RECORDS, a mesh-file chunk at slot 0x23. */
typedef enum map_mesh_animation_record_type {
    MAP_MESH_ANIMATION_RECORD_END = 0,
    MAP_MESH_ANIMATION_RECORD_KEYFRAMES = 1,
    MAP_MESH_ANIMATION_RECORD_INSTRUCTIONS = 2,
    MAP_MESH_ANIMATION_RECORD_PART_PROPERTIES = 3,
} map_mesh_animation_record_type_e;

/* Record header parsed by battle_map_load_mesh_animation_record: a 32-bit type,
 * an entry count and the packed entries. */
typedef struct map_state_record {
    u32 type;  /* map_mesh_animation_record_type_e */
    u32 count; /* types 1/3: entry count; type 2: packed instruction/state dimensions */
    u8 data[4];
} map_state_record_t;

/* resource_type selects a serialized prefix or block; file offsets locate
 * variable-length chunks within the loaded resource. */
typedef union map_resource {
    map_mesh_geometry_t geometry;
    map_polygon_flags_resource_t polygon_flags;
    map_mesh_file_header_t mesh_file;
    map_light_resource_t lighting;
    map_terrain_resource_t terrain;
    map_texture_animation_t texture_animations[MAP_TEXTURE_ANIMATION_COUNT];
    map_state_record_t state_record;
    gns_record_t gns_records[150];
    gns_command_record_prefix_t command;
    s16 command_halfwords[10]; /* 16-bit view of the 20-byte command prefix. */
    clut_t clut;
    u32 texture_words[0]; /* Variable-length image data consumed by LoadImage. */
    u8 bytes[0];          /* Byte offsets in files and variable-length command payloads. */
} map_resource_t;

/* Resources read but not yet applied: the palette and lighting data wait for
 * MAP_RESOURCE_APPLY_PALETTES and MAP_RESOURCE_APPLY_LIGHTING, and the texture flags mark the whole
 * texture or the quarters read into the load buffer, uploaded at MAP_LOAD_STATE_APPLY_TEXTURE. */
extern u8 g_battle_map_palette_pending;
extern u8 g_battle_map_pending_palette_data[0x200];
extern u8 g_battle_map_lighting_pending;
extern u8 g_battle_map_pending_lighting_data[0x2d];
extern u8 g_battle_map_texture_pending;
extern u8 g_battle_map_texture_quarter_0_pending;
extern u8 g_battle_map_texture_quarter_1_pending;
extern u8 g_battle_map_texture_quarter_2_pending;
extern u8 g_battle_map_texture_quarter_3_pending;

/* loading */
/* Commands of battle_map_load_data, read only while the GNS phase is idle. */
typedef enum map_load_command {
    MAP_LOAD_COMMAND_LOAD_GNS = 0x73, /* always read the GNS; no current caller issues this */
    MAP_LOAD_COMMAND_START = 0x75,
    MAP_LOAD_COMMAND_POLL = 0x76,       /* an unhandled command only advances the load */
    MAP_LOAD_COMMAND_START_HELD = 0x99, /* as START, but hold dispatch until MAP_DATA_COMMAND_RELEASE_GNS_HOLD */
    MAP_LOAD_COMMAND_POLL_HELD = 0x9c,
} map_load_command_e;

/* g_battle_map_resource_load_state; these are not the loader's normal return values. */
typedef enum map_resource_load_state {
    MAP_LOAD_STATE_IDLE = 0x00,
    MAP_LOAD_STATE_GNS_FILE = 0x74, /* also accepted as a no-op input command */
    MAP_LOAD_STATE_DONE = 0x77,
    MAP_LOAD_STATE_RESOURCE_FILE = 0x78,
    MAP_LOAD_STATE_MESH_RECORDS = 0x79, /* rebuild render records once per buffer */
    MAP_LOAD_STATE_MESH_RECORDS_SECOND = 0x91,
    MAP_LOAD_STATE_APPLY_LIGHTING = 0x92,
    MAP_LOAD_STATE_APPLY_TEXTURE = 0x93,
} map_resource_load_state_e;

/* The loader otherwise returns g_battle_map_gns_load_phase. */
enum { MAP_LOAD_RESULT_HELD = 0x9b };

/* g_battle_map_gns_load_phase and the normal return of battle_map_load_data. */
typedef enum map_gns_load_phase {
    MAP_GNS_LOAD_PHASE_IDLE = 0,
    MAP_GNS_LOAD_PHASE_RESOURCES = 1, /* loading the pending resource files one by one */
    MAP_GNS_LOAD_PHASE_START = 15,    /* choose the resources once the GNS is in */
} map_gns_load_phase_e;

extern map_resource_t* g_battle_map_data_load_buffer;
extern s32 g_battle_deployment_finished;
extern s32 g_battle_map_deep_dungeon_load_in_progress;
extern char g_battle_map_directory_path[];
extern u32 g_battle_map_gns_lba[MAP_ID_COUNT]; /* 0 where the map id has no file */
extern u32 g_battle_map_gns_record_cursor;
extern gns_record_t g_battle_map_gns_records[150];
extern map_resource_t* g_battle_map_mesh_load_buffer;
extern s32 g_battle_map_mesh_load_state;
extern s32 g_battle_map_resource_load_state; /* map_resource_load_state_e */
extern gns_record_t g_battle_map_selected_mesh_record;
extern u16 g_battle_map_append_state;
/* 1 starts, 2 polls, >=3 is reset to zero by the outer update. */
extern u16 g_battle_map_reload_state;
/* GNS records selected for loading and the ones currently loaded, indexed by
 * resource type. */
extern gns_record_t g_battle_map_pending_gns_records[GNS_RESOURCE_TABLE_COUNT];
extern gns_record_t g_battle_map_loaded_gns_records[GNS_RESOURCE_TABLE_COUNT];
extern u8 g_battle_map_gns_dispatch_held;
extern s32 g_battle_map_gns_load_phase; /* map_gns_load_phase_e */
extern s32 g_battle_map_loaded_id;
extern RECT g_battle_map_vram_load_rect;

s32 battle_map_dispatch_gns_resource(u8 resource_type, map_resource_t* data);
s32 battle_map_start_data_load(void);
s32 battle_map_poll_data_load(void);
s32 battle_map_step_init_sequence(s32 map_id, s32 step);
s32 battle_map_load_data(s32 map_id, s32 command);
s32 battle_map_apply_gns_command(s32 phase, map_resource_t* record);
s16 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state);
void* battle_map_start_file_load(
    const char* unused_directory_path, const gns_load_info_t* load_info, void* destination, s32 unused_resource_type);
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running);
s32 battle_map_load_alternate_mesh(s32 palette_mode);
gns_record_t* battle_map_load_gns_and_move_find_items(s32 map_id);

#endif

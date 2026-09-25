#ifndef FFT_MAP_H
#define FFT_MAP_H

#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Low six bits of a terrain tile's first byte. */
typedef enum map_surface_type {
    MAP_SURFACE_NATURAL = 0x00,
    MAP_SURFACE_SAND = 0x01,
    MAP_SURFACE_STALACTITE = 0x02,
    MAP_SURFACE_GRASSLAND = 0x03,
    MAP_SURFACE_THICKET = 0x04,
    MAP_SURFACE_SNOW = 0x05,
    MAP_SURFACE_ROCKY_CLIFF = 0x06,
    MAP_SURFACE_GRAVEL = 0x07,
    MAP_SURFACE_WASTELAND = 0x08,
    MAP_SURFACE_SWAMP = 0x09,
    MAP_SURFACE_MARSH = 0x0a,
    MAP_SURFACE_POISONED_MARSH = 0x0b,
    MAP_SURFACE_LAVA_ROCKS = 0x0c,
    MAP_SURFACE_ICE = 0x0d,
    MAP_SURFACE_WATERWAY = 0x0e,
    MAP_SURFACE_RIVER = 0x0f,
    MAP_SURFACE_LAKE = 0x10,
    MAP_SURFACE_SEA = 0x11,
    MAP_SURFACE_LAVA = 0x12,
    MAP_SURFACE_ROAD = 0x13,
    MAP_SURFACE_WOODEN_FLOOR = 0x14,
    MAP_SURFACE_STONE_FLOOR = 0x15,
    MAP_SURFACE_ROOF = 0x16,
    MAP_SURFACE_STONE_WALL = 0x17,
    MAP_SURFACE_SKY = 0x18,
    MAP_SURFACE_DARKNESS = 0x19,
    MAP_SURFACE_SALT = 0x1a,
    MAP_SURFACE_BOOK = 0x1b,
    MAP_SURFACE_OBSTACLE = 0x1c,
    MAP_SURFACE_RUG = 0x1d,
    MAP_SURFACE_TREE = 0x1e,
    MAP_SURFACE_BOX = 0x1f,
    MAP_SURFACE_BRICK = 0x20,
    MAP_SURFACE_CHIMNEY = 0x21,
    MAP_SURFACE_MUD_WALL = 0x22,
    MAP_SURFACE_BRIDGE = 0x23,
    MAP_SURFACE_WATER_PLANT = 0x24,
    MAP_SURFACE_STAIRS = 0x25,
    MAP_SURFACE_FURNITURE = 0x26,
    MAP_SURFACE_IVY = 0x27,
    MAP_SURFACE_DECK = 0x28,
    MAP_SURFACE_MACHINE = 0x29,
    MAP_SURFACE_IRON_PLATE = 0x2a,
    MAP_SURFACE_MOSS = 0x2b,
    MAP_SURFACE_TOMBSTONE = 0x2c,
    MAP_SURFACE_WATERFALL = 0x2d,
    MAP_SURFACE_COFFIN = 0x2e,
    MAP_SURFACE_CROSS_SECTION = 0x3f,
} map_surface_type_e;

enum { MAP_SURFACE_MASK = 0x3f };

/* Selectors accepted by battle_map_get_tile_data_value and its setter twin.
 * Selector 14 is unsupported; setter selector 15 performs a separate operation
 * whose relationship to tile data is not established. */
typedef enum map_tile_data_selector {
    MAP_TILE_DATA_SURFACE_TYPE = 0,
    MAP_TILE_DATA_UNKNOWN_01 = 1,
    MAP_TILE_DATA_HEIGHT = 2,
    MAP_TILE_DATA_HALF_HEIGHT = 3,
    MAP_TILE_DATA_DEPTH = 4,
    MAP_TILE_DATA_SLOPE_TYPE = 5,
    MAP_TILE_DATA_CEILING_DEPTH = 6,
    MAP_TILE_DATA_BLOCKED = 7,
    MAP_TILE_DATA_UNTARGETABLE = 8,
    MAP_TILE_DATA_SHADOW_MODE = 9,
    MAP_TILE_DATA_UNKNOWN_FLAGS_4_6 = 10,
    MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK = 11,
    MAP_TILE_DATA_MOVE_DESTINATION_MARK = 12,
    MAP_TILE_DATA_ABILITY_RANGE_MARK = 13,
} map_tile_data_selector_e;

enum {
    MAP_TILE_CEILING_DEPTH_MASK = 0x1f,
    MAP_TILE_FLAG_MOVE_DESTINATION = 0x20,
    MAP_TILE_FLAG_MOVE_DESTINATION_SHIFT = 5,
    MAP_TILE_FLAG_ABILITY_RANGE = 0x40,
    MAP_TILE_FLAG_ABILITY_RANGE_SHIFT = 6,
    MAP_TILE_FLAG_TARGETED = 0x80,
};

enum {
    MAP_TILE_FLAG_BLOCKED = 0x01,
    MAP_TILE_FLAG_UNTARGETABLE = 0x02,
    MAP_TILE_FLAG_UNTARGETABLE_SHIFT = 1,
    MAP_TILE_COLLISION_MASK = 0x03,
    MAP_TILE_SHADOW_MODE_SHIFT = 2,
    MAP_TILE_SHADOW_MODE_MASK = 0x0c,
    MAP_TILE_SHADOW_MODE_VALUE_MASK = 0x03,
    MAP_TILE_FLAG_CANNOT_STOP = 0x80,
};

enum {
    MAP_TILE_HALF_HEIGHT_MASK = 0x1f,
    MAP_TILE_DEPTH_VALUE_MASK = 0x07,
    MAP_TILE_DEPTH_SHIFT = 5,
    MAP_TILE_DEPTH_MASK = 0xe0,
};

typedef enum map_tile_camera_block_direction {
    MAP_TILE_CAMERA_BLOCK_NORTHWEST = 0x01,
    MAP_TILE_CAMERA_BLOCK_SOUTHWEST = 0x02,
    MAP_TILE_CAMERA_BLOCK_SOUTHEAST = 0x04,
    MAP_TILE_CAMERA_BLOCK_NORTHEAST = 0x08,
} map_tile_camera_block_direction_e;

enum {
    MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK = 0x0f,
    MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT = 4,
    MAP_TILE_CAMERA_BLOCK_STEEP_MASK = 0xf0,
};

typedef enum gns_map_state_comparison {
    GNS_MAP_STATE_EQUAL = 1,
    GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD = 2,
    GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD = 3,
} gns_map_state_comparison_e;

typedef enum gns_resource_type {
    GNS_RESOURCE_TEXTURE = 0x17,
    GNS_RESOURCE_MESH_PRIMARY = 0x2e,
    GNS_RESOURCE_MESH_OVERRIDE = 0x2f,
    GNS_RESOURCE_MESH_ALTERNATE = 0x30,
    GNS_RESOURCE_END = 0x31,
    GNS_RESOURCE_SET_INDOOR = 0x85,
    GNS_RESOURCE_SET_OUTDOOR = 0x86,
    GNS_RESOURCE_CLEAR_SNOW = 0x88,
    GNS_RESOURCE_SET_SNOW = 0x89,
    GNS_RESOURCE_RESERVED_8A = 0x8a,
    GNS_RESOURCE_SET_WEATHER_MODIFIER = 0x8b,
} gns_resource_type_e;

/* Commands of battle_map_dispatch_map_data_command, which switches on command
 * - 0x6a. This is its own id space: event Use3DObject (0x54) reaches command
 * 0x80. Names come from the case bodies. */
typedef enum map_data_command {
    MAP_DATA_COMMAND_DISABLE_TEXTURE_ANIMATION = 0x6a,
    MAP_DATA_COMMAND_RESTORE_TEXTURE_ANIMATION = 0x6b,
    MAP_DATA_COMMAND_DISABLE_ALL_TEXTURE_ANIMATIONS = 0x71,
    MAP_DATA_COMMAND_RESTORE_ALL_TEXTURE_ANIMATIONS = 0x72,
    MAP_DATA_COMMAND_DISABLE_OVERLAY_DITHER = 0x7a, /* SetDrawMode dtd 0 */
    MAP_DATA_COMMAND_ENABLE_OVERLAY_DITHER = 0x7b,  /* SetDrawMode dtd 1 */
    MAP_DATA_COMMAND_SET_3D_OBJECT_STATE = 0x80,
    MAP_DATA_COMMAND_GET_3D_OBJECT_STATE = 0x81,
    MAP_DATA_COMMAND_GET_TEXTURE_ANIMATION_ACTIVE = 0x82,
    MAP_DATA_COMMAND_START_TEXTURE_ANIMATION = 0x83,
    MAP_DATA_COMMAND_SPLIT_MESH_PARTS = 0x98,
    MAP_DATA_COMMAND_RELEASE_GNS_HOLD = 0x9a,
    MAP_DATA_COMMAND_SET_TEXTURE_ANIMATION_DURATION = 0x9d,
} map_data_command_e;

/* Commands of battle_map_light_state_command. The 0x63-0x65 bodies access
 * g_battle_map_light_direction, but every caller passes or reads the map
 * darkness colour there, so the commands are named for the callers. */
typedef enum map_light_command {
    MAP_LIGHT_COMMAND_SET_BACKGROUND_GRADIENT = 0x56,
    MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT = 0x57,
    MAP_LIGHT_COMMAND_SAVE_BACKGROUND_GRADIENT = 0x58,
    MAP_LIGHT_COMMAND_RESTORE_BACKGROUND_GRADIENT = 0x59,
    MAP_LIGHT_COMMAND_SET_AMBIENT_COLOR = 0x5a,
    MAP_LIGHT_COMMAND_SAVE_AMBIENT_COLOR = 0x5b,
    MAP_LIGHT_COMMAND_RESTORE_AMBIENT_COLOR = 0x5c,
    MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR = 0x5d,
    MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR = 0x63,
    MAP_LIGHT_COMMAND_RESET_DARKNESS_COLOR = 0x64,
    MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR = 0x65,
} map_light_command_e;

enum {
    MAP_PALETTE_ROW_COUNT = 16,
    MAP_PALETTE_COLORS_PER_ROW = 16,
    MAP_PALETTE_COLOR_COUNT = 256,
};

/* Eight runtime slots rotate the palette colors for map texture animations.
 * They are separate from the 32 serialized 0x14-byte animation instructions
 * described by the Maps/Mesh format. */
enum {
    MAP_TEXTURE_ANIMATION_CAPACITY = 8,
    MAP_TEXTURE_ANIMATION_STATE_BYTES = 0x0a,
};

/* RGB888 in a four-byte runtime slot, not the packed three-byte map-file
 * format. The background gradient, ambient light and darkness colours all use
 * it: the scenario VM copies RGB into bytes 0..2 (and 4..6 for the second
 * gradient colour), and 0x800901f8 copies each complete word with lwl/lwr.
 * Preserve byte alignment. */
typedef struct map_color {
    u8 red;
    u8 green;
    u8 blue;
    u8 _unknown03; /* Copied by the helper; not an established alpha channel. */
} map_color_t;

typedef struct map_background_gradient_colors {
    map_color_t first;
    map_color_t second;
} map_background_gradient_colors_t;

typedef char map_background_gradient_colors_size_must_be_8[(sizeof(map_background_gradient_colors_t) == 8) ? 1 : -1];

typedef union map_tile_surface {
    u8 value;
    struct {
        u8 type : 6; /* map_surface_type_e */
        u8 unknown : 2;
    } bits;
} map_tile_surface_t;

enum {
    MAP_TILE_SURFACE_FLAG_6 = 0x40,
};

typedef union map_tile_flags_06 {
    u8 value;
    struct {
        u8 blocked : 1;
        u8 untargetable : 1;
        u8 shadow_mode : 2;
        u8 unknown_4_6 : 3;
        u8 cannot_stop : 1;
    } bits;
} map_tile_flags_06_t;

/* Eight-byte terrain record: battle_map_get_tile_data_pointer (0x80183fb4)
 * indexes (layer * 256 + y * width + x) * 8. Scenario opcode 0x40 at
 * BATTLE 0x801451c8 / WORLD 0x800f84f0 modifies bytes 0, 2 and 6.
 * Only the low six surface bits have an established meaning here. Opcode
 * 0x40 replaces the untargetable bit and clears the blocked bit. Its bitfield
 * accesses reproduce the separate surface-byte
 * reload and flag-update order at BATTLE 0x801451e4..0x80145244. */
typedef struct map_tile {
    map_tile_surface_t surface; /* 0x00 */
    u8 _unknown01;
    u8 height; /* 0x02; tile height in whole units */
    /* 0x03; bits 0..4 slope half-height, bits 5..7 depth added to height
     * (battle_map_get_tile_data_value selectors 3/4, movement geometry setup). */
    u8 depth_half_height;
    u8 slope_type;              /* 0x04; two bits per exit side (battle_move_calculate_tile_ceiling) */
    u8 ceiling_depth_and_marks; /* 0x05; low five bits are ceiling depth; high bits are transient map marks */
    map_tile_flags_06_t flags_06;
    /* 0x07; low nibble blocks camera directions at a flat pitch; high nibble
     * holds the same direction bits for a steep pitch. */
    u8 camera_block_masks;
} map_tile_t;

/* Bitfield view of map_tile_t.depth_half_height, for the readers the plain u8
 * spelling cannot reproduce.
 *
 * Reading the byte as a u8 and masking lets GCC fold the two fields into one
 * load and compare the slope with sltiu. The target issues two separate lbu
 * reads and a signed slti (BATTLE 0x80184550 / 0x80184564), which only the
 * bitfield spelling emits.
 *
 * This is deliberately a separate type rather than a change to map_tile_t:
 * that field has more than ten readers whose matches depend on the u8
 * spelling. Reach it as
 * ((map_tile_depth_t*)&tile->depth_half_height)->bits.half_height. */
typedef union map_tile_depth {
    u8 packed;
    struct {
        u8 half_height : 5; /* slope half-height */
        u8 depth : 3;       /* depth added to height */
    } bits;
} map_tile_depth_t;

typedef char map_tile_depth_size_must_be_1[(sizeof(map_tile_depth_t) == 1) ? 1 : -1];

typedef char map_tile_size_must_be_8[(sizeof(map_tile_t) == 8) ? 1 : -1];

/* Battle terrain: two layers of 256 records, indexed
 * (layer * 256 + y * width + x). Callers that walk it with raw byte offsets
 * cast to u8*; the record shape is map_tile_t. */
extern map_tile_t g_battle_map_tile_data[];

typedef enum map_move_find_item_entry_flags {
    MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR = 0x01,
    MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP = 0x02,
    MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS = 0x04,
    MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE = 0x08,
    MAP_MOVE_FIND_ENTRY_NO_TRAP = 0x10,
    MAP_MOVE_FIND_ENTRY_NO_ITEM = 0x20,
    MAP_MOVE_FIND_ENTRY_UPPER_LAYER = 0x80,
} map_move_find_item_entry_flags_e;

typedef struct map_move_find_item_entry {
    u8 position; /* X in the high nibble, Y in the low nibble. */
    u8 flags;    /* map_move_find_item_entry_flags_e */
    u8 rare_item_id;
    u8 common_item_id;
} map_move_find_item_entry_t;

typedef struct map_move_find_item_data {
    map_move_find_item_entry_t entries[4];
} map_move_find_item_data_t;

typedef char map_move_find_item_entry_size_must_be_4[(sizeof(map_move_find_item_entry_t) == 4) ? 1 : -1];
typedef char map_move_find_item_data_size_must_be_16[(sizeof(map_move_find_item_data_t) == 16) ? 1 : -1];

/* Unscaled components decoded from one PlayStation ABBBBBGGGGGRRRRR word. */
typedef struct map_palette_color_components {
    u8 red_5bit;
    u8 green_5bit;
    u8 blue_5bit;
    u8 alpha_bit;
    u8 red_delta_biased;   /* target - current + 0x1f */
    u8 green_delta_biased; /* target - current + 0x1f */
    u8 blue_delta_biased;  /* target - current + 0x1f */
} map_palette_color_components_t;

typedef struct map_palette_animation_state {
    u8 active;
    u8 blend_step;    /* 0x01; index into g_battle_map_palette_blend_steps_32/_8 */
    u8 delay_counter; /* 0x02; one blend step each time it reaches mode >> 2 */
    u8 mode;
    u8 _unknown04[4];
} map_palette_animation_state_t;

/* One 0x982-byte runtime bank per loaded mesh, based at 0x800995f4.
 * Components and packed_colors are runtime state/mirror, not the separate
 * 0x200-byte-per-mesh upload staging at 0x800e4ea4. Palette extraction fills
 * both; 0x80092f98 uploads staging and clears the pending word at 0x800995ec. */
typedef struct map_palette_runtime_bank {
    u8 enabled;
    u8 updates_disabled;
    map_palette_animation_state_t animations[MAP_PALETTE_ROW_COUNT];
    map_palette_color_components_t components[MAP_PALETTE_COLOR_COUNT];
    u16 packed_colors[MAP_PALETTE_COLOR_COUNT];
} map_palette_runtime_bank_t;

typedef char map_palette_color_components_size_must_be_7[(sizeof(map_palette_color_components_t) == 7) ? 1 : -1];

typedef char map_palette_animation_state_size_must_be_8[(sizeof(map_palette_animation_state_t) == 8) ? 1 : -1];

typedef char map_palette_runtime_bank_size_must_be_0x982[(sizeof(map_palette_runtime_bank_t) == 0x982) ? 1 : -1];

/* Palette runtime state block at 0x800995ec: battle_map_update_animations
 * addresses the banks (0x800995f4) from this base. The upload/move words are
 * the pending flags battle_map_update_lighting (0x80092f98) services. */
typedef struct map_palette_state {
    s32 upload_pending;
    s32 move_pending;
    map_palette_runtime_bank_t banks[14];
} map_palette_state_t;

typedef char map_palette_state_size_must_be_0x8524[(sizeof(map_palette_state_t) == 0x8524) ? 1 : -1];

extern map_palette_state_t g_battle_map_palette_state;

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
typedef struct gns_file_record {
    u16 script_variable_id;  /* 0x00 */
    s16 map_state;           /* 0x02 */
    u8 map_state_comparison; /* 0x04; gns_map_state_comparison_e */
    u8 resource_type;        /* 0x05; gns_resource_type_e */
    u16 padding_3333;        /* 0x06 */
    u32 disc_lba;            /* 0x08; absolute, including the high halfword at +0x0a */
    u32 byte_length;         /* 0x0c; sector-rounded */
    u8 padding_55_66_77[3];  /* 0x10 */
    u8 file_present_marker;  /* 0x13; 0x88 on disc, tested for nonzero */
} gns_file_record_t;

/* Command rows after the 0x80 separator reuse the 20-byte prefix, but +0x08
 * is a byte stride (0x14 or 0x1c), not a disc LBA. The 0x1c rows have eight
 * more payload bytes after this prefix. +0x06/+0x09..+0x0b are zero in the
 * USA disc's command rows. */
typedef struct gns_command_record_prefix {
    u16 script_variable_id;  /* 0x00 */
    s16 map_state;           /* 0x02 */
    u8 map_state_comparison; /* 0x04 */
    u8 resource_type;        /* 0x05 */
    u16 reserved_06;
    u8 record_byte_length; /* 0x08 */
    u8 reserved_09;
    u16 reserved_0a;
    u8 payload[8]; /* 0x0c; command arguments, followed by more bytes for 0x1c rows */
} gns_command_record_prefix_t;

/* Mesh files begin with 49 32-bit byte offsets indexed by resource ID (slot = ID * 4).
 * The dispatcher visits IDs 0x10..0x30; slots 0..0x0f and the other reserved
 * words are zero in the USA disc's mesh files. A zero offset means the chunk
 * is absent. MAP000.5 alone has an embedded texture quarter at 0x4c. */
typedef struct map_mesh_file_header {
    u32 unused_resource_offsets_00_0f[16]; /* 0x00 */
    u32 geometry_offset;                   /* 0x40 */
    u32 color_palette_offset;              /* 0x44 */
    u32 reserved_48;                       /* 0x48 */
    u32 texture_quarter_0_offset;          /* 0x4c */
    u32 reserved_50[5];                    /* 0x50 */
    u32 lights_background_offset;          /* 0x64 */
    u32 terrain_offset;                    /* 0x68 */
    u32 texture_animation_offset;          /* 0x6c */
    u32 palette_animation_offset;          /* 0x70 */
    u32 reserved_74[2];                    /* 0x74 */
    u32 grayscale_palette_offset;          /* 0x7c */
    u32 reserved_80[3];                    /* 0x80 */
    u32 mesh_animation_offset;             /* 0x8c */
    u32 animated_mesh_offsets[8];          /* 0x90 */
    u32 polygon_render_properties_offset;  /* 0xb0 */
    u32 reserved_b4[4];                    /* 0xb4 */
} map_mesh_file_header_t;

typedef char gns_file_record_size_must_be_0x14[(sizeof(gns_file_record_t) == 0x14) ? 1 : -1];
typedef char gns_command_record_prefix_size_must_be_0x14[(sizeof(gns_command_record_prefix_t) == 0x14) ? 1 : -1];
typedef char map_mesh_file_header_size_must_be_0xc4[(sizeof(map_mesh_file_header_t) == 0xc4) ? 1 : -1];

typedef union battle_map_mesh_terrain_tile {
    u16 packed;
    s16 reset_value;
} battle_map_mesh_terrain_tile_t;

/* Polygon position and normal records copied from the mesh geometry block.
 * Each XYZ vector occupies eight bytes; the first position's spare halfword
 * stores the packed terrain tile. */
typedef struct battle_map_mesh_triangle_positions {
    s16 x0, y0, z0;
    battle_map_mesh_terrain_tile_t terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _pad16;
} battle_map_mesh_triangle_positions_t;

typedef struct battle_map_mesh_quad_positions {
    s16 x0, y0, z0;
    battle_map_mesh_terrain_tile_t terrain_tile;
    s16 x1, y1, z1;
    s16 polygon_flags;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} battle_map_mesh_quad_positions_t;

typedef struct battle_map_mesh_triangle_normals {
    s16 x0, y0, z0;
    u16 _pad06;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
} battle_map_mesh_triangle_normals_t;

typedef struct battle_map_mesh_quad_normals {
    s16 x0, y0, z0;
    u16 _pad06;
    s16 x1, y1, z1;
    u16 _pad0e;
    s16 x2, y2, z2;
    u16 _pad16;
    s16 x3, y3, z3;
    u16 _pad1e;
} battle_map_mesh_quad_normals_t;

/* Per-part start indices and counts stored after the mesh transform data. */
typedef struct battle_map_mesh_part_metadata {
    u8 _unknown00[0x88];
    u16 textured_triangle_start;
    u16 textured_quad_start;
    u16 untextured_triangle_start;
    u16 untextured_quad_start;
    u16 textured_triangle_count;
    u16 textured_quad_count;
    u16 untextured_triangle_count;
    u16 untextured_quad_count;
} battle_map_mesh_part_metadata_t;

/* Halfword-aligned view of a 20-byte GNS record: the target copies these
 * records with lwl/lwr pairs, so the loader's type has no word member. */
typedef struct gns_record_view {
    s16 script_variable_id;
    s16 map_state;
    u8 map_state_comparison;
    u8 resource_type;
    u8 load_info[14];
} gns_record_view_t;

typedef char gns_record_view_size_must_be_0x14[(sizeof(gns_record_view_t) == 0x14) ? 1 : -1];

/* GNS records selected for loading and the ones currently loaded, indexed by
 * resource type. */
extern gns_record_view_t g_battle_map_pending_gns_records[49];
extern gns_record_view_t g_battle_map_loaded_gns_records[49];

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
extern map_background_gradient_colors_t g_battle_event_background_colors;
/* The applied background gradient pair at 0x801251c8. The copies at
 * 0x800e81d0, 0x800e8228 and 0x800e827c move each colour as an independent
 * unaligned word (lwl/lwr + swl/swr). */
extern map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern map_color_t g_battle_map_ambient_light_color;
extern map_color_t g_battle_map_darkness_color;
extern map_background_gradient_colors_t g_world_event_background_colors;

void battle_map_transition_ambient_light_color(s32 frame_duration, const map_color_t* color);
void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_transition_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_transition_darkness_color(s32 frame_duration, const map_color_t* color);

/* Map data, palette and mesh interfaces implemented in the battle overlay. */
void battle_map_init_darkness(s32 unused_0, s32 unused_1, s32 map_id);
s32 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state);
void battle_map_load_palette_data(const u16* palette, s32 mesh_index, s32 palette_row_index, s32 load_all_rows);
s32 battle_map_start_file_load(void* unused_file_table, u8* load_info, s32 destination, s32 suppress_loading_display);
s32 battle_map_stop_texture_animations_and_weather_sfx(void);
void battle_map_store_max_coordinates(void* destination);
void battle_map_update_palette_colors(
    s32 mode, s32 palette_group, s32 palette_index, s32 update_all, const u16* colors);

/* Runtime colour fades stepped by update_map_animations (0x800912a4):
 * g_map_background_gradient_transition at 0x800a1b10, the ambient light at
 * 0x800a1b50 and the darkness at 0x800a1b74. Channels are 16.16 fixed point;
 * the byte fields follow the updater's use. */
typedef struct map_color_transition_channels {
    s32 red;
    s32 green;
    s32 blue;
    s32 red_step;
    s32 green_step;
    s32 blue_step;
} map_color_transition_channels_t;

typedef struct map_color_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 _unknown04[4];
    map_color_transition_channels_t channels;
} map_color_transition_t;

/* The background gradient fade: one channel set per gradient colour. colors[]
 * at 0x38 (0x800a1b48/0x800a1b4c) is the applied gradient pair;
 * battle_map_blend_background_gradient_color indexes it from the same base. */
typedef struct map_gradient_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 _unknown04[4];
    map_color_transition_channels_t channels[2];
    map_color_t colors[2];
} map_gradient_transition_t;

typedef char
    map_color_transition_channels_size_must_be_0x18[(sizeof(map_color_transition_channels_t) == 0x18) ? 1 : -1];
typedef char map_color_transition_size_must_be_0x20[(sizeof(map_color_transition_t) == 0x20) ? 1 : -1];
typedef char map_gradient_transition_size_must_be_0x40[(sizeof(map_gradient_transition_t) == 0x40) ? 1 : -1];

extern map_gradient_transition_t g_map_background_gradient_transition;
extern map_color_transition_t g_battle_map_ambient_light_transition;
extern map_color_transition_t g_battle_map_darkness_transition;

/* 0x98-byte map mesh part records at 0x800fbe00. The +0x88..+0x96 start
 * and count halfwords are map_mesh_part_metadata_t's (0x800f4dd4). The
 * vector triples are the rotation, translation and scale channels; element
 * [1] is the current value, [0] the start of the running keyframe and [2]
 * its target or step. */
typedef struct battle_map_mesh_part {
    SVECTOR vectors_a[3]; /* 0x00 */
    SVECTOR vectors_b[3]; /* 0x18 */
    VECTOR vectors_c[3];  /* 0x30; reset to {0x1000, 0x1000, 0x1000} */
    MATRIX matrix_60;     /* 0x60 */
    s16 value_80;         /* 0x80 */
    s16 value_82;         /* 0x82 */
    u8 value_84;          /* 0x84 */
    u8 value_85;          /* 0x85 */
    u8 parent;            /* 0x86; parent part, from mesh properties byte 0 */
    u8 value_87;          /* 0x87 */
    u16 starts[4];        /* 0x88 */
    u16 counts[4];        /* 0x90 */
} battle_map_mesh_part_t;

typedef char battle_map_mesh_part_size_must_be_0x98[(sizeof(battle_map_mesh_part_t) == 0x98) ? 1 : -1];

extern battle_map_mesh_part_t g_battle_map_mesh_parts[9];

/* One 0x50-byte map mesh animation keyframe (MAPnnn.GNS record type 1).
 * Keyframe ids are 1-based; entry 0 is never loaded. Each of the nine channels
 * (rotation, translation and scale of a mesh part) has a target value, a
 * start/end angle for the sine modes and a mode flag word whose bits 0 and 1
 * are TweenTo and TweenBy. */
typedef struct battle_map_mesh_keyframe {
    SVECTOR vector_0;   /* 0x00; rotation */
    SVECTOR vector_8;   /* 0x08; translation */
    s16 value_10;       /* 0x10; scale x */
    s16 value_12;       /* 0x12; scale y */
    s16 value_14;       /* 0x14; scale z */
    s16 _unknown_16;    /* 0x16 */
    s16 angle_start[9]; /* 0x18 */
    s16 angle_end[9];   /* 0x2a */
    u16 flags[9];       /* 0x3c */
    u16 _unknown_4e;    /* 0x4e */
} battle_map_mesh_keyframe_t;

typedef char battle_map_mesh_keyframe_size_must_be_0x50[(sizeof(battle_map_mesh_keyframe_t) == 0x50) ? 1 : -1];

/* One mesh animation instruction (MAPnnn.GNS record type 2). A duration of
 * 0 stops the part's animation when it is reached. */
typedef struct battle_map_mesh_instruction {
    u8 keyframe;  /* 1-based keyframe id */
    u8 next;      /* index of the following instruction */
    u16 duration; /* frames */
} battle_map_mesh_instruction_t;

/* A mesh part's 8 animation states of 17 instructions (index 0 unused).
 * States are loaded state-major; row 0 is also the running copy that event
 * command 0x80 overwrites with the selected state. */
typedef struct battle_map_mesh_instructions {
    battle_map_mesh_instruction_t states[8][17];
} battle_map_mesh_instructions_t;

typedef char
    battle_map_mesh_instructions_size_must_be_0x220[(sizeof(battle_map_mesh_instructions_t) == 0x220) ? 1 : -1];

extern battle_map_mesh_keyframe_t g_battle_map_mesh_animation_keyframes[];
extern battle_map_mesh_instructions_t g_battle_map_mesh_animation_instructions[9];

/* Provisional runtime copy of one 0x14-byte map texture animation
 * instruction (32 rows).
 * UV animations move a canvas-sized frame through VRAM with the DR_MOVE
 * packet of the same index; palette animations reload CLUT row
 * canvas_x / 16 from the palette frames at g_battle_map_palette_animation_frames. */
typedef struct battle_map_texture_animation {
    s16 canvas_x;   /* 0x00 */
    s16 canvas_y;   /* 0x02 */
    s16 width;      /* 0x04 */
    s16 height;     /* 0x06 */
    s16 frame_x;    /* 0x08 */
    s16 frame_y;    /* 0x0a */
    u8 column;      /* 0x0c */
    u8 row;         /* 0x0d */
    u8 mode;        /* 0x0e: animation technique / playback state */
    u8 frame_count; /* 0x0f */
    u8 frame;       /* 0x10 */
    u8 duration;    /* 0x11 */
    u8 timer;       /* 0x12 */
    u8 active;      /* 0x13 */
} battle_map_texture_animation_t;

typedef char battle_map_texture_animation_size_must_be_0x14[(sizeof(battle_map_texture_animation_t) == 0x14) ? 1 : -1];

extern battle_map_texture_animation_t g_battle_map_texture_animations[32];
extern DR_MOVE g_battle_map_texture_animation_moves[32];
extern SVECTOR g_battle_map_camera_rotation;

/* Lightning flash state at 0x800f79c0 (g_battle_map_lightning_state).
 * battle_map_set_light_matrices fills color_matrices[0] and [2]; the lightning weather commands clear the rotation part
 * of color_matrices[1] and start the 60-frame flash timer. */
typedef struct battle_lightning_state {
    u32 timer;                  /* 0x00 */
    MATRIX color_matrices[3];   /* 0x04 */
    u8 _unknown64[0x20];        /* 0x64 */
    map_color_t base_colors[2]; /* 0x84 */
    map_color_t colors[2];      /* 0x8c */
} battle_lightning_state_t;

typedef char battle_lightning_state_size_must_be_0x94[(sizeof(battle_lightning_state_t) == 0x94) ? 1 : -1];

extern battle_lightning_state_t g_battle_map_lightning_state;

/* Record header parsed by the map-state loader at 0x800f26bc: a type word,
 * an entry count and the packed entries. */
typedef struct battle_map_state_record {
    u32 type;
    u32 count;
    u8 data[4];
} battle_map_state_record_t;

u8* battle_map_load_saved_state_record(battle_map_state_record_t* record);

u8* battle_map_light_state_command(s32 command, u8* data);
/* Provisional 0xa-byte selected-tile record: BATTLE's at 0x8014d02c, filled by
 * battle_map_store_selected_tile_data, and WORLD's at 0x8013a310, filled by
 * world_map_store_selected_tile_data and saved whole by
 * world_gfx_copy_screen_setup_out.
 * unknown_02/unknown_04 are the two bytes of the per-surface-type main table
 * D_8005E950 ("movement cost/geomancy/movement support tables" in
 * scus_94221_datatables.txt); their meaning is unproven. */
typedef struct map_selected_tile {
    s16 surface_type;   /* 0x00 */
    s16 unknown_02;     /* 0x02 */
    s16 unknown_04;     /* 0x04 */
    u16 display_height; /* 0x06: half height + height * 2 + depth * 2 */
    s16 depth;          /* 0x08 */
} map_selected_tile_t;

void battle_map_append_mesh_geometry(u16* geometry_data, battle_map_mesh_part_metadata_t* metadata);
void battle_map_copy_xy_coords_and_tile_data(u8* p);
map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id);
map_tile_t* battle_map_get_tile_data_ptr_from_misc_screen_coords(u32 misc_id);
s32 battle_map_get_weather_severity(void);
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running);
s32 battle_map_load_mesh_variant(s32 mesh_slot);
void battle_map_queue_textured_quads(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_textured_triangles(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_untextured_quads(u32* otag, void* prims, s32 depth, s32* count);
void battle_map_queue_untextured_triangles(u32* otag, void* prims, s32 depth, s32* count);
s32 battle_map_refresh_deep_dungeon(void);
s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b);
s32 battle_map_command_start_texture_animation(s32 value_a, s32 value_b);
void battle_map_set_light_matrices(MATRIX* first, MATRIX* second);
void battle_map_set_weather_texture_overlay(s32 command);
s32 battle_map_start_texture_animation(s32 frame_duration, s32 polygon_group, s32 first_polygon, s32 last_polygon);
void battle_map_update_mesh_part_animations(void);
void battle_map_update_palette_from_other(
    const u16* source, s32 section_id, s32 unit_misc_id, s32 preset, s16 red, s16 green, s16 blue, s32 final_value);
void battle_map_update_texture_animations(void);
extern u8 g_battle_map_ambient_polygon_color[3];
extern u8 g_battle_map_back_color_bytes[3];
extern SVECTOR g_battle_map_command_0x96_rotation;
extern u8 g_battle_map_gns_dispatch_held;
extern s32 g_battle_map_gns_load_phase;
extern u8 g_battle_map_light_direction[4];
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start colour matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */
extern MATRIX
    g_battle_map_light_transition_target_color_matrix; /* transition target (0x66) or delta (0x68) colour matrix */
extern MATRIX
    g_battle_map_light_transition_target_light_matrix; /* transition target (0x66) or delta (0x68) light matrix */
extern u8 g_battle_map_lighting_pending;
extern s32 g_battle_map_lightning_mode;
extern s32 g_battle_map_loaded_id;
extern MATRIX g_battle_map_local_light_matrix;
extern s32 g_battle_map_lowest_surface_y;
extern s32 g_battle_map_mesh_part_animation_states[];
extern u8* g_battle_map_mesh_part_geometry[9];
extern u16 g_battle_map_palette_animation_frames[][16];
extern u8 g_battle_map_palette_blend_steps_32[][32];
extern u8 g_battle_map_palette_blend_steps_8[][8];
extern u8 g_battle_map_palette_pending;
extern u32 g_battle_map_palette_upload_words[];
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
extern battle_map_mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern battle_map_mesh_quad_positions_t g_battle_map_textured_quad_positions[];
extern battle_map_mesh_triangle_positions_t g_battle_map_untextured_triangle_positions[];
extern battle_map_mesh_quad_positions_t g_battle_map_untextured_quad_positions[];
extern battle_map_mesh_triangle_normals_t g_battle_map_textured_triangle_normals[];
extern battle_map_mesh_quad_normals_t g_battle_map_textured_quad_normals[];
extern u8 g_battle_map_pending_lighting_data[0x2d];
extern u8 g_battle_map_pending_palette_data[0x200];
extern u8 g_battle_map_polygon_flag_clear_countdown;
extern s32 g_battle_map_primary_textured_quad_count;
extern s32 g_battle_map_primary_textured_triangle_count;
extern s32 g_battle_map_primary_untextured_quad_count;
extern s32 g_battle_map_primary_untextured_triangle_count;
extern s32 g_battle_map_saved_back_color_blue;
extern s32 g_battle_map_saved_back_color_green;
extern s32 g_battle_map_saved_back_color_red;
extern map_color_t g_battle_map_saved_background_gradient_first;
extern map_color_t g_battle_map_saved_background_gradient_second;
extern MATRIX g_battle_map_saved_light_color_matrix; /* saved colour matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;       /* saved light matrix (command 0x5f/0x60) */
extern u8 g_battle_map_saved_texture_animation_modes[];
extern u8 g_battle_map_texture_pending;
extern u8 g_battle_map_texture_quarter_0_pending;
extern u8 g_battle_map_texture_quarter_1_pending;
extern u8 g_battle_map_texture_quarter_2_pending;
extern u8 g_battle_map_texture_quarter_3_pending;
extern map_tile_t* g_battle_map_tile_data_ptr;
extern u8 g_battle_map_tile_depth;
extern u8 g_battle_map_tile_width;
extern RECT g_battle_map_vram_load_rect;
extern u8 g_battle_map_weather_fall_speed;
/* Map weather flags: bit 1 suppresses weather, bit 0 marks snow. */
extern s32 g_battle_map_weather_flags;
extern MATRIX g_battle_map_weather_matrix;
extern SVECTOR g_battle_map_weather_rotation;
extern VECTOR g_battle_map_weather_scale;
extern s32 g_battle_map_weather_sprite_scale;
extern VECTOR g_battle_map_weather_translation;

void battle_map_init_mesh_render_records(u16* geometry_data);
void battle_map_init_weather_particles(s32 command);
gns_file_record_t* battle_map_load_gns_and_move_find_items(s32 map_id);
void battle_map_reset_mesh_render_state(void);

#endif

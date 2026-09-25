#ifndef FFT_MAP_H
#define FFT_MAP_H

/* Map tile and surface records shared by BATTLE and WORLD. */

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
typedef char map_tile_size_must_be_8[(sizeof(map_tile_t) == 8) ? 1 : -1];

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

#endif

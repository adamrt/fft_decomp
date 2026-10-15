#ifndef MAP_H
#define MAP_H

/* Tactical-level data shared by BATTLE and WORLD: map IDs, terrain tiles and
 * surface records. The map engine is declared in battle_map.h. */

#include "psx/libgte.h"
#include "psx/types.h"

/* Horizontal X/Z extent of one terrain tile in map geometry units. */
enum { MAP_TILE_SIZE = 28 };

/* Vertical map geometry units per whole terrain-height step. */
enum { MAP_HEIGHT_STEP = 12 };

/* Terrain storage has two fixed-size layers, independent of the map dimensions. */
enum {
    MAP_TERRAIN_LAYER_COUNT = 2,
    MAP_TILES_PER_LAYER = 256,
    MAP_TILE_COUNT = MAP_TERRAIN_LAYER_COUNT * MAP_TILES_PER_LAYER,
};

/* coordinates */
/* Map x, terrain layer and map y, shared by unit placement and obstacle tests. */
typedef struct map_tile_coordinates {
    s16 x;     /* 0x00 */
    s16 layer; /* 0x02 */
    s16 y;     /* 0x04 */
} map_tile_coordinates_t;
typedef char map_tile_coordinates_size_must_be_6[sizeof(map_tile_coordinates_t) == 6 ? 1 : -1];

/* maps */
/* Battle map ids: EVENT_SCRIPT_VAR_CURRENT_MAP and the index of
 * g_map_gns_lba. Each is named as its map name text spells it
 * (TEXT_ID_MAP_NAME_BASE + id - 1); a map whose name repeats an earlier one
 * ends in its id. The ten floors of the Deep Dungeon
 * (g_wldcore_deep_dungeon_battles) have one-word names and take that prefix.
 * Maps 0, 116-119 and 125 have a file on the disc and no name; 120-124, 126
 * and 127 have neither. */
typedef enum map_id {
    MAP_ID_AT_MAIN_GATE_OF_IGROS_CASTLE = 1,
    MAP_ID_BACK_GATE_OF_LESALIA_CASTLE = 2,
    MAP_ID_HALL_OF_ST_MUROND_TEMPLE = 3,
    MAP_ID_OFFICE_OF_LESALIA_CASTLE = 4,
    MAP_ID_ROOF_OF_RIOVANES_CASTLE = 5,
    MAP_ID_AT_THE_GATE_OF_RIOVANES_CASTLE = 6,
    MAP_ID_INSIDE_OF_RIOVANES_CASTLE = 7,
    MAP_ID_RIOVANES_CASTLE = 8,
    MAP_ID_CITADEL_OF_IGROS_CASTLE = 9,
    MAP_ID_INSIDE_OF_IGROS_CASTLE = 10,
    MAP_ID_OFFICE_OF_IGROS_CASTLE = 11,
    MAP_ID_AT_THE_GATE_OF_LIONEL_CASTLE = 12,
    MAP_ID_INSIDE_OF_LIONEL_CASTLE = 13,
    MAP_ID_OFFICE_OF_LIONEL_CASTLE = 14,
    MAP_ID_AT_THE_GATE_OF_LIMBERRY_CASTLE = 15,
    MAP_ID_INSIDE_OF_LIMBERRY_CASTLE = 16,
    MAP_ID_UNDERGROUND_CEMETERY_OF_LIMBERRY_CASTLE = 17,
    MAP_ID_OFFICE_OF_LIMBERRY_CASTLE = 18,
    MAP_ID_AT_THE_GATE_OF_LIMBERRY_CASTLE_19 = 19,
    MAP_ID_INSIDE_OF_ZELTENNIA_CASTLE = 20,
    MAP_ID_ZELTENNIA_CASTLE = 21,
    MAP_ID_MAGIC_CITY_GARILAND = 22,
    MAP_ID_BEOULVE_RESIDENCE = 23,
    MAP_ID_MILITARY_ACADEMYS_AUDITORIUM = 24,
    MAP_ID_YARDOW_FORT_CITY = 25,
    MAP_ID_WEAPON_STORAGE_OF_YARDOW = 26,
    MAP_ID_GOLAND_COAL_CITY = 27,
    MAP_ID_COLLIERY_UNDERGROUND_FIRST_FLOOR = 28,
    MAP_ID_COLLIERY_UNDERGROUND_SECOND_FLOOR = 29,
    MAP_ID_COLLIERY_UNDERGROUND_THIRD_FLOOR = 30,
    MAP_ID_DORTER_TRADE_CITY = 31,
    MAP_ID_SLUMS_IN_DORTER = 32,
    MAP_ID_HOSPITAL_IN_SLUMS = 33,
    MAP_ID_CELLAR_OF_SAND_MOUSE = 34,
    MAP_ID_ZALAND_FORT_CITY = 35,
    MAP_ID_CHURCH_OUTSIDE_THE_TOWN = 36,
    MAP_ID_RUINS_OUTSIDE_ZALAND = 37,
    MAP_ID_GOUG_MACHINE_CITY = 38,
    MAP_ID_UNDERGROUND_PASSAGE_IN_GOLAND = 39,
    MAP_ID_SLUMS_IN_GOUG = 40,
    MAP_ID_BESRODIOS_HOUSE = 41,
    MAP_ID_WARJILIS_TRADE_CITY = 42,
    MAP_ID_PORT_OF_WARJILIS = 43,
    MAP_ID_BERVENIA_FREE_CITY = 44,
    MAP_ID_RUINS_OF_ZELTENNIA_CASTLES_CHURCH = 45,
    MAP_ID_CEMETERY_OF_HEAVENLY_KNIGHT_BALBANES = 46,
    MAP_ID_ZARGHIDAS_TRADE_CITY = 47,
    MAP_ID_SLUMS_OF_ZARGHIDAS = 48,
    MAP_ID_FORT_ZEAKDEN = 49,
    MAP_ID_ST_MUROND_TEMPLE = 50,
    MAP_ID_ST_MUROND_TEMPLE_51 = 51,
    MAP_ID_CHAPEL_OF_ST_MUROND_TEMPLE = 52,
    MAP_ID_ENTRANCE_TO_DEATH_CITY = 53,
    MAP_ID_LOST_SACRED_PRECINCTS = 54,
    MAP_ID_GRAVEYARD_OF_AIRSHIPS = 55,
    MAP_ID_ORBONNE_MONASTERY = 56,
    MAP_ID_UNDERGROUND_BOOK_STORAGE_FIRST_FLOOR = 57,
    MAP_ID_UNDERGROUND_BOOK_STORAGE_SECOND_FLOOR = 58,
    MAP_ID_UNDERGROUND_BOOK_STORAGE_THIRD_FLOOR = 59,
    MAP_ID_UNDERGROUND_BOOK_STORAGE_FOURTH_FLOOR = 60,
    MAP_ID_UNDERGROUND_BOOK_STORAGE_FIFTH_FLOOR = 61,
    MAP_ID_CHAPEL_OF_ORBONNE_MONASTERY = 62,
    MAP_ID_GOLGORAND_EXECUTION_SITE = 63,
    MAP_ID_IN_FRONT_OF_BETHLA_GARRISONS_SLUICE = 64,
    MAP_ID_GRANARY_OF_BETHLA_GARRISON = 65,
    MAP_ID_SOUTH_WALL_OF_BETHLA_GARRISON = 66,
    MAP_ID_NORTH_WALL_OF_BETHLA_GARRISON = 67,
    MAP_ID_BETHLA_GARRISON = 68,
    MAP_ID_MUROND_DEATH_CITY = 69,
    MAP_ID_NELVESKA_TEMPLE = 70,
    MAP_ID_DOLBODAR_SWAMP = 71,
    MAP_ID_FOVOHAM_PLAINS = 72,
    MAP_ID_INSIDE_OF_WINDMILL_SHED = 73,
    MAP_ID_SWEEGY_WOODS = 74,
    MAP_ID_BERVENIA_VOLCANO = 75,
    MAP_ID_ZEKLAUS_DESERT = 76,
    MAP_ID_LENALIA_PLATEAU = 77,
    MAP_ID_ZIGOLIS_SWAMP = 78,
    MAP_ID_YUGUO_WOODS = 79,
    MAP_ID_ARAGUAY_WOODS = 80,
    MAP_ID_GROG_HILL = 81,
    MAP_ID_BED_DESERT = 82,
    MAP_ID_ZIREKILE_FALLS = 83,
    MAP_ID_BARIAUS_HILL = 84,
    MAP_ID_MANDALIA_PLAINS = 85,
    MAP_ID_DOGUOLA_PASS = 86,
    MAP_ID_BARIAUS_VALLEY = 87,
    MAP_ID_FINATH_RIVER = 88,
    MAP_ID_POESKAS_LAKE = 89,
    MAP_ID_GERMINAS_PEAK = 90,
    MAP_ID_THIEVES_FORT = 91,
    MAP_ID_IGROS_BEOULVE_RESIDENCE = 92,
    MAP_ID_BROKE_DOWN_SHED_WOODEN_BUILDING = 93,
    MAP_ID_BROKE_DOWN_SHED_STONE_BUILDING = 94,
    MAP_ID_CHURCH = 95,
    MAP_ID_PUB = 96,
    MAP_ID_INSIDE_CASTLE_GATE_IN_LESALIA = 97,
    MAP_ID_OUTSIDE_CASTLE_GATE_IN_LESALIA = 98,
    MAP_ID_MAIN_STREET_OF_LESALIA = 99,
    MAP_ID_PUBLIC_CEMETARY = 100,
    MAP_ID_FOR_TUTORIAL_1 = 101,
    MAP_ID_FOR_TUTORIAL_2 = 102,
    MAP_ID_WINDMILL_SHED = 103,
    MAP_ID_A_ROOM_OF_BEOULVE_RESIDENCE = 104,
    MAP_ID_DEEP_DUNGEON_TERMINATE = 105,
    MAP_ID_DEEP_DUNGEON_DELTA = 106,
    MAP_ID_DEEP_DUNGEON_NOGIAS = 107,
    MAP_ID_DEEP_DUNGEON_VOYAGE = 108,
    MAP_ID_DEEP_DUNGEON_BRIDGE = 109,
    MAP_ID_DEEP_DUNGEON_VALKYRIES = 110,
    MAP_ID_DEEP_DUNGEON_MLAPAN = 111,
    MAP_ID_DEEP_DUNGEON_TIGER = 112,
    MAP_ID_DEEP_DUNGEON_HORROR = 113,
    MAP_ID_DEEP_DUNGEON_END = 114,
    MAP_ID_BANISHED_FORT = 115,
    MAP_ID_COUNT = 128,
} map_id_e;

/* terrain */
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

/* Selectors accepted by map_get_tile_data_value and its setter twin.
 * Selector 14 is unsupported; setter selector 15 performs a separate operation
 * whose relationship to tile data is not established. No caller passes
 * selector 1 or 10. */
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
    MAP_TILE_DATA_SHADING = 9,
    MAP_TILE_DATA_UNKNOWN_FLAGS_4_6 = 10,
    MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK = 11,
    MAP_TILE_DATA_MOVE_DESTINATION_MARK = 12,
    MAP_TILE_DATA_ABILITY_RANGE_MARK = 13,
} map_tile_data_selector_e;

/* Depth-bias flag in terrain tile byte0; the low six bits use MAP_SURFACE_MASK. */
enum {
    MAP_TILE_SURFACE_FLAG_DEPTH_BIAS = 0x40,
};

typedef struct map_tile_surface {
    u8 type : 6;       /* map_surface_type_e */
    u8 depth_bias : 1; /* camera depth offset 5 instead of 1 for the cursor, units and shadows on this tile */
    u8 _unused_00_bit7 : 1;
} map_tile_surface_t;

/* Packed mark flags in terrain tile byte 0x05. */
enum {
    MAP_TILE_FLAG_MOVE_DESTINATION = 0x20,
    MAP_TILE_FLAG_ABILITY_RANGE = 0x40,
    MAP_TILE_FLAG_TARGETED = 0x80,
};

/* Tile byte 0x05. Retail reads the depth as a bitfield: its zero test and
 * its value are separate loads (BATTLE 0x801af948..0x801af970). */
typedef struct map_tile_marks {
    u8 ceiling_depth : 5;
    u8 move_destination : 1;
    u8 ability_range : 1;
    u8 targeted : 1;
} map_tile_marks_t;

/* Packed collision flags in terrain tile byte 0x06. */
enum {
    MAP_TILE_FLAG_BLOCKED = 0x01,
    MAP_TILE_FLAG_UNTARGETABLE = 0x02,
    MAP_TILE_COLLISION_MASK = 0x03,
    MAP_TILE_FLAG_CANNOT_STOP = 0x80,
};

/* Terrain shading darkens the unit palette by 0, 4, 8 or 12 per channel. */
typedef enum map_tile_shading {
    MAP_TILE_SHADING_NORMAL = 0,
    MAP_TILE_SHADING_DARK = 1,
    MAP_TILE_SHADING_DARKER = 2,
    MAP_TILE_SHADING_DARKEST = 3,
} map_tile_shading_e;

typedef struct map_tile_flags {
    u8 blocked : 1;
    u8 untargetable : 1;
    u8 shading : 2;
    /* Values 0-5 in the terrain files; copied to the unread trajectory obstacle result. */
    u8 _unknown_00 : 3;
    u8 cannot_stop : 1; /* set by no terrain file */
} map_tile_flags_t;

enum {
    MAP_TILE_HALF_HEIGHT_MASK = 0x1f,
    MAP_TILE_DEPTH_VALUE_MASK = 0x07,
    MAP_TILE_DEPTH_SHIFT = 5,
};

/* Tile byte 0x03: five-bit slope half-height and three-bit depth. Both
 * contribute to the height used by movement and targeting geometry. */
typedef struct map_tile_depth {
    u8 half_height : 5;
    u8 depth : 3;
} map_tile_depth_t;

/* map_tile_t.slope_type shapes. The byte holds the height of each side in two bits (east, west,
 * south and north from bit 0): 0 low, 1 sloping, 2 high. An edge rises towards the side it names;
 * a low corner has one raised corner and a high corner has one lowered corner opposite the one it names. */
typedef enum map_slope_type {
    MAP_SLOPE_LOW_CORNER_SOUTH_EAST = 0x11,
    MAP_SLOPE_LOW_CORNER_SOUTH_WEST = 0x14,
    MAP_SLOPE_EDGE_SOUTH = 0x25,
    MAP_SLOPE_LOW_CORNER_NORTH_EAST = 0x41,
    MAP_SLOPE_LOW_CORNER_NORTH_WEST = 0x44,
    MAP_SLOPE_EDGE_EAST = 0x52,
    MAP_SLOPE_EDGE_WEST = 0x58,
    MAP_SLOPE_HIGH_CORNER_SOUTH_EAST = 0x66,
    MAP_SLOPE_HIGH_CORNER_SOUTH_WEST = 0x69,
    MAP_SLOPE_EDGE_NORTH = 0x85,
    MAP_SLOPE_HIGH_CORNER_NORTH_EAST = 0x96,
    MAP_SLOPE_HIGH_CORNER_NORTH_WEST = 0x99,
} map_slope_type_e;

/* Direction bits shared by the flat and steep nibbles of tile byte 0x07. */
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

/* Eight-byte terrain record: map_get_tile_data_pointer (0x80183fb4)
 * indexes (layer * 256 + y * width + x) * 8. Scenario opcode 0x40 at
 * BATTLE 0x801451c8 / WORLD 0x800f84f0 modifies bytes 0, 2 and 6.
 * Opcode 0x40 replaces the untargetable bit and clears the blocked bit. Its
 * bitfield accesses reproduce the separate surface-byte reload and
 * flag-update order at BATTLE 0x801451e4..0x80145244. */
typedef struct map_tile {
    map_tile_surface_t surface;               /* 0x00 */
    u8 _unknown_01;                           /* 0x01; 0 in every terrain file; reached only by tile data selector 1 */
    u8 height;                                /* 0x02; tile height in whole units */
    map_tile_depth_t depth_half_height;       /* 0x03 */
    u8 slope_type;                            /* 0x04; map_slope_type_e: two bits per exit side */
    map_tile_marks_t ceiling_depth_and_marks; /* 0x05; ceiling depth and the transient map marks */
    map_tile_flags_t flags;
    /* 0x07; low nibble blocks camera directions at a flat pitch; high nibble
     * holds the same direction bits for a steep pitch. */
    u8 camera_block_masks;
} map_tile_t;
typedef char map_tile_size_must_be_8[(sizeof(map_tile_t) == 8) ? 1 : -1];

/* selection */
/* Provisional 0xa-byte selected-tile record: BATTLE's at 0x8014d02c, filled by
 * map_store_selected_tile_data, and WORLD's at 0x8013a310, filled by
 * map_store_selected_tile_data and saved whole by
 * menu_copy_billboard_data_out.
 * _unknown_02/_unknown_04 are the two bytes of the per-surface-type main table
 * g_main_terrain_tile_info_pairs, 25 and 25 for all 64 surface types; nothing
 * reads them back. */
typedef struct map_selected_tile {
    s16 surface_type;   /* 0x00 */
    s16 _unknown_02;    /* 0x02 */
    s16 _unknown_04;    /* 0x04 */
    s16 display_height; /* 0x06: half height + height * 2 + depth * 2 */
    s16 depth;          /* 0x08 */
} map_selected_tile_t;

/* background */
/* Map backgrounds use two four-byte runtime color slots. The fourth byte
 * is copied with each CVECTOR but is not an alpha channel; mesh-file RGB
 * triples have a separate three-byte layout. */
typedef enum map_background_gradient_color {
    MAP_BACKGROUND_GRADIENT_TOP = 0,
    MAP_BACKGROUND_GRADIENT_BOTTOM = 1,
    MAP_BACKGROUND_GRADIENT_COLOR_COUNT = 2,
} map_background_gradient_color_e;

typedef struct map_background_gradient_colors {
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];
} map_background_gradient_colors_t;
typedef char map_background_gradient_colors_size_must_be_8[(sizeof(map_background_gradient_colors_t) == 8) ? 1 : -1];

/* script state */
/* Map-destruction handshake between the event interpreter and the frame loop. */
extern s16 g_map_destruction_wait_state;

/* The raw weather script variable, before indoor and snow flags apply. */
s32 map_get_weather(void);

/* map state */
extern map_selected_tile_t g_map_selected_tile_data;
extern u16 g_map_refresh_pending;

/* unreferenced stubs, neutral names */
s32 map_return_zero_1(void);

/* per-module bodies: each module links its own copy under the shared name. */
void map_reset_script_light_transform_variant(void);

/* shared between programs */
extern map_background_gradient_colors_t g_event_background_colors;

#endif

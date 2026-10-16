#ifndef MAP_H
#define MAP_H

/* Map data and state shared by BATTLE and WORLD: map ids, terrain tiles, the selected tile, background colors and
 * the event library's map routines. The battle map engine is declared in battle_map.h. */

#include "psx/libgte.h"
#include "psx/types.h"

/* geometry */
/* Horizontal X/Z extent of one terrain tile in map geometry units. */
enum { BATTLE_MAP_TILE_SIZE = 28 };

/* Vertical map geometry units per whole terrain-height step. */
enum { BATTLE_MAP_HEIGHT_STEP = 12 };

/* Terrain storage has two fixed-size layers, independent of the map dimensions. */
enum {
    BATTLE_MAP_TERRAIN_LAYER_COUNT = 2,
    BATTLE_MAP_TILES_PER_LAYER = 256,
    BATTLE_MAP_TILE_COUNT = BATTLE_MAP_TERRAIN_LAYER_COUNT * BATTLE_MAP_TILES_PER_LAYER,
};

/* Map x, terrain layer and map y, shared by unit placement and obstacle tests. */
typedef struct battle_map_tile_coordinates {
    s16 x;     /* 0x00 */
    s16 layer; /* 0x02 */
    s16 y;     /* 0x04 */
} battle_map_tile_coordinates_t;
typedef char battle_map_tile_coordinates_size_must_be_6[sizeof(battle_map_tile_coordinates_t) == 6 ? 1 : -1];

/* maps */
/* Battle map ids: EVENT_SCRIPT_VAR_CURRENT_MAP and the index of
 * g_battle_map_gns_lba. Each is named as its map name text spells it
 * (TEXT_ID_MAP_NAME_BASE + id - 1); a map whose name repeats an earlier one
 * ends in its id. The ten floors of the Deep Dungeon
 * (g_deep_dungeon_battles) have one-word names and take that prefix.
 * Maps 0, 116-119 and 125 have a file on the disc and no name; 120-124, 126
 * and 127 have neither. */
typedef enum map_id {
    BATTLE_MAP_ID_AT_MAIN_GATE_OF_IGROS_CASTLE = 1,
    BATTLE_MAP_ID_BACK_GATE_OF_LESALIA_CASTLE = 2,
    BATTLE_MAP_ID_HALL_OF_ST_MUROND_TEMPLE = 3,
    BATTLE_MAP_ID_OFFICE_OF_LESALIA_CASTLE = 4,
    BATTLE_MAP_ID_ROOF_OF_RIOVANES_CASTLE = 5,
    BATTLE_MAP_ID_AT_THE_GATE_OF_RIOVANES_CASTLE = 6,
    BATTLE_MAP_ID_INSIDE_OF_RIOVANES_CASTLE = 7,
    BATTLE_MAP_ID_RIOVANES_CASTLE = 8,
    BATTLE_MAP_ID_CITADEL_OF_IGROS_CASTLE = 9,
    BATTLE_MAP_ID_INSIDE_OF_IGROS_CASTLE = 10,
    BATTLE_MAP_ID_OFFICE_OF_IGROS_CASTLE = 11,
    BATTLE_MAP_ID_AT_THE_GATE_OF_LIONEL_CASTLE = 12,
    BATTLE_MAP_ID_INSIDE_OF_LIONEL_CASTLE = 13,
    BATTLE_MAP_ID_OFFICE_OF_LIONEL_CASTLE = 14,
    BATTLE_MAP_ID_AT_THE_GATE_OF_LIMBERRY_CASTLE = 15,
    BATTLE_MAP_ID_INSIDE_OF_LIMBERRY_CASTLE = 16,
    BATTLE_MAP_ID_UNDERGROUND_CEMETERY_OF_LIMBERRY_CASTLE = 17,
    BATTLE_MAP_ID_OFFICE_OF_LIMBERRY_CASTLE = 18,
    BATTLE_MAP_ID_AT_THE_GATE_OF_LIMBERRY_CASTLE_19 = 19,
    BATTLE_MAP_ID_INSIDE_OF_ZELTENNIA_CASTLE = 20,
    BATTLE_MAP_ID_ZELTENNIA_CASTLE = 21,
    BATTLE_MAP_ID_MAGIC_CITY_GARILAND = 22,
    BATTLE_MAP_ID_BEOULVE_RESIDENCE = 23,
    BATTLE_MAP_ID_MILITARY_ACADEMYS_AUDITORIUM = 24,
    BATTLE_MAP_ID_YARDOW_FORT_CITY = 25,
    BATTLE_MAP_ID_WEAPON_STORAGE_OF_YARDOW = 26,
    BATTLE_MAP_ID_GOLAND_COAL_CITY = 27,
    BATTLE_MAP_ID_COLLIERY_UNDERGROUND_FIRST_FLOOR = 28,
    BATTLE_MAP_ID_COLLIERY_UNDERGROUND_SECOND_FLOOR = 29,
    BATTLE_MAP_ID_COLLIERY_UNDERGROUND_THIRD_FLOOR = 30,
    BATTLE_MAP_ID_DORTER_TRADE_CITY = 31,
    BATTLE_MAP_ID_SLUMS_IN_DORTER = 32,
    BATTLE_MAP_ID_HOSPITAL_IN_SLUMS = 33,
    BATTLE_MAP_ID_CELLAR_OF_SAND_MOUSE = 34,
    BATTLE_MAP_ID_ZALAND_FORT_CITY = 35,
    BATTLE_MAP_ID_CHURCH_OUTSIDE_THE_TOWN = 36,
    BATTLE_MAP_ID_RUINS_OUTSIDE_ZALAND = 37,
    BATTLE_MAP_ID_GOUG_MACHINE_CITY = 38,
    BATTLE_MAP_ID_UNDERGROUND_PASSAGE_IN_GOLAND = 39,
    BATTLE_MAP_ID_SLUMS_IN_GOUG = 40,
    BATTLE_MAP_ID_BESRODIOS_HOUSE = 41,
    BATTLE_MAP_ID_WARJILIS_TRADE_CITY = 42,
    BATTLE_MAP_ID_PORT_OF_WARJILIS = 43,
    BATTLE_MAP_ID_BERVENIA_FREE_CITY = 44,
    BATTLE_MAP_ID_RUINS_OF_ZELTENNIA_CASTLES_CHURCH = 45,
    BATTLE_MAP_ID_CEMETERY_OF_HEAVENLY_KNIGHT_BALBANES = 46,
    BATTLE_MAP_ID_ZARGHIDAS_TRADE_CITY = 47,
    BATTLE_MAP_ID_SLUMS_OF_ZARGHIDAS = 48,
    BATTLE_MAP_ID_FORT_ZEAKDEN = 49,
    BATTLE_MAP_ID_ST_MUROND_TEMPLE = 50,
    BATTLE_MAP_ID_ST_MUROND_TEMPLE_51 = 51,
    BATTLE_MAP_ID_CHAPEL_OF_ST_MUROND_TEMPLE = 52,
    BATTLE_MAP_ID_ENTRANCE_TO_DEATH_CITY = 53,
    BATTLE_MAP_ID_LOST_SACRED_PRECINCTS = 54,
    BATTLE_MAP_ID_GRAVEYARD_OF_AIRSHIPS = 55,
    BATTLE_MAP_ID_ORBONNE_MONASTERY = 56,
    BATTLE_MAP_ID_UNDERGROUND_BOOK_STORAGE_FIRST_FLOOR = 57,
    BATTLE_MAP_ID_UNDERGROUND_BOOK_STORAGE_SECOND_FLOOR = 58,
    BATTLE_MAP_ID_UNDERGROUND_BOOK_STORAGE_THIRD_FLOOR = 59,
    BATTLE_MAP_ID_UNDERGROUND_BOOK_STORAGE_FOURTH_FLOOR = 60,
    BATTLE_MAP_ID_UNDERGROUND_BOOK_STORAGE_FIFTH_FLOOR = 61,
    BATTLE_MAP_ID_CHAPEL_OF_ORBONNE_MONASTERY = 62,
    BATTLE_MAP_ID_GOLGORAND_EXECUTION_SITE = 63,
    BATTLE_MAP_ID_IN_FRONT_OF_BETHLA_GARRISONS_SLUICE = 64,
    BATTLE_MAP_ID_GRANARY_OF_BETHLA_GARRISON = 65,
    BATTLE_MAP_ID_SOUTH_WALL_OF_BETHLA_GARRISON = 66,
    BATTLE_MAP_ID_NORTH_WALL_OF_BETHLA_GARRISON = 67,
    BATTLE_MAP_ID_BETHLA_GARRISON = 68,
    BATTLE_MAP_ID_MUROND_DEATH_CITY = 69,
    BATTLE_MAP_ID_NELVESKA_TEMPLE = 70,
    BATTLE_MAP_ID_DOLBODAR_SWAMP = 71,
    BATTLE_MAP_ID_FOVOHAM_PLAINS = 72,
    BATTLE_MAP_ID_INSIDE_OF_WINDMILL_SHED = 73,
    BATTLE_MAP_ID_SWEEGY_WOODS = 74,
    BATTLE_MAP_ID_BERVENIA_VOLCANO = 75,
    BATTLE_MAP_ID_ZEKLAUS_DESERT = 76,
    BATTLE_MAP_ID_LENALIA_PLATEAU = 77,
    BATTLE_MAP_ID_ZIGOLIS_SWAMP = 78,
    BATTLE_MAP_ID_YUGUO_WOODS = 79,
    BATTLE_MAP_ID_ARAGUAY_WOODS = 80,
    BATTLE_MAP_ID_GROG_HILL = 81,
    BATTLE_MAP_ID_BED_DESERT = 82,
    BATTLE_MAP_ID_ZIREKILE_FALLS = 83,
    BATTLE_MAP_ID_BARIAUS_HILL = 84,
    BATTLE_MAP_ID_MANDALIA_PLAINS = 85,
    BATTLE_MAP_ID_DOGUOLA_PASS = 86,
    BATTLE_MAP_ID_BARIAUS_VALLEY = 87,
    BATTLE_MAP_ID_FINATH_RIVER = 88,
    BATTLE_MAP_ID_POESKAS_LAKE = 89,
    BATTLE_MAP_ID_GERMINAS_PEAK = 90,
    BATTLE_MAP_ID_THIEVES_FORT = 91,
    BATTLE_MAP_ID_IGROS_BEOULVE_RESIDENCE = 92,
    BATTLE_MAP_ID_BROKE_DOWN_SHED_WOODEN_BUILDING = 93,
    BATTLE_MAP_ID_BROKE_DOWN_SHED_STONE_BUILDING = 94,
    BATTLE_MAP_ID_CHURCH = 95,
    BATTLE_MAP_ID_PUB = 96,
    BATTLE_MAP_ID_INSIDE_CASTLE_GATE_IN_LESALIA = 97,
    BATTLE_MAP_ID_OUTSIDE_CASTLE_GATE_IN_LESALIA = 98,
    BATTLE_MAP_ID_MAIN_STREET_OF_LESALIA = 99,
    BATTLE_MAP_ID_PUBLIC_CEMETARY = 100,
    BATTLE_MAP_ID_FOR_TUTORIAL_1 = 101,
    BATTLE_MAP_ID_FOR_TUTORIAL_2 = 102,
    BATTLE_MAP_ID_WINDMILL_SHED = 103,
    BATTLE_MAP_ID_A_ROOM_OF_BEOULVE_RESIDENCE = 104,
    BATTLE_MAP_ID_DEEP_DUNGEON_TERMINATE = 105,
    BATTLE_MAP_ID_DEEP_DUNGEON_DELTA = 106,
    BATTLE_MAP_ID_DEEP_DUNGEON_NOGIAS = 107,
    BATTLE_MAP_ID_DEEP_DUNGEON_VOYAGE = 108,
    BATTLE_MAP_ID_DEEP_DUNGEON_BRIDGE = 109,
    BATTLE_MAP_ID_DEEP_DUNGEON_VALKYRIES = 110,
    BATTLE_MAP_ID_DEEP_DUNGEON_MLAPAN = 111,
    BATTLE_MAP_ID_DEEP_DUNGEON_TIGER = 112,
    BATTLE_MAP_ID_DEEP_DUNGEON_HORROR = 113,
    BATTLE_MAP_ID_DEEP_DUNGEON_END = 114,
    BATTLE_MAP_ID_BANISHED_FORT = 115,
    BATTLE_MAP_ID_COUNT = 128,
} battle_map_id_e;

/* terrain */
/* Low six bits of a terrain tile's first byte. */
typedef enum battle_map_surface_type {
    BATTLE_MAP_SURFACE_NATURAL = 0x00,
    BATTLE_MAP_SURFACE_SAND = 0x01,
    BATTLE_MAP_SURFACE_STALACTITE = 0x02,
    BATTLE_MAP_SURFACE_GRASSLAND = 0x03,
    BATTLE_MAP_SURFACE_THICKET = 0x04,
    BATTLE_MAP_SURFACE_SNOW = 0x05,
    BATTLE_MAP_SURFACE_ROCKY_CLIFF = 0x06,
    BATTLE_MAP_SURFACE_GRAVEL = 0x07,
    BATTLE_MAP_SURFACE_WASTELAND = 0x08,
    BATTLE_MAP_SURFACE_SWAMP = 0x09,
    BATTLE_MAP_SURFACE_MARSH = 0x0a,
    BATTLE_MAP_SURFACE_POISONED_MARSH = 0x0b,
    BATTLE_MAP_SURFACE_LAVA_ROCKS = 0x0c,
    BATTLE_MAP_SURFACE_ICE = 0x0d,
    BATTLE_MAP_SURFACE_WATERWAY = 0x0e,
    BATTLE_MAP_SURFACE_RIVER = 0x0f,
    BATTLE_MAP_SURFACE_LAKE = 0x10,
    BATTLE_MAP_SURFACE_SEA = 0x11,
    BATTLE_MAP_SURFACE_LAVA = 0x12,
    BATTLE_MAP_SURFACE_ROAD = 0x13,
    BATTLE_MAP_SURFACE_WOODEN_FLOOR = 0x14,
    BATTLE_MAP_SURFACE_STONE_FLOOR = 0x15,
    BATTLE_MAP_SURFACE_ROOF = 0x16,
    BATTLE_MAP_SURFACE_STONE_WALL = 0x17,
    BATTLE_MAP_SURFACE_SKY = 0x18,
    BATTLE_MAP_SURFACE_DARKNESS = 0x19,
    BATTLE_MAP_SURFACE_SALT = 0x1a,
    BATTLE_MAP_SURFACE_BOOK = 0x1b,
    BATTLE_MAP_SURFACE_OBSTACLE = 0x1c,
    BATTLE_MAP_SURFACE_RUG = 0x1d,
    BATTLE_MAP_SURFACE_TREE = 0x1e,
    BATTLE_MAP_SURFACE_BOX = 0x1f,
    BATTLE_MAP_SURFACE_BRICK = 0x20,
    BATTLE_MAP_SURFACE_CHIMNEY = 0x21,
    BATTLE_MAP_SURFACE_MUD_WALL = 0x22,
    BATTLE_MAP_SURFACE_BRIDGE = 0x23,
    BATTLE_MAP_SURFACE_WATER_PLANT = 0x24,
    BATTLE_MAP_SURFACE_STAIRS = 0x25,
    BATTLE_MAP_SURFACE_FURNITURE = 0x26,
    BATTLE_MAP_SURFACE_IVY = 0x27,
    BATTLE_MAP_SURFACE_DECK = 0x28,
    BATTLE_MAP_SURFACE_MACHINE = 0x29,
    BATTLE_MAP_SURFACE_IRON_PLATE = 0x2a,
    BATTLE_MAP_SURFACE_MOSS = 0x2b,
    BATTLE_MAP_SURFACE_TOMBSTONE = 0x2c,
    BATTLE_MAP_SURFACE_WATERFALL = 0x2d,
    BATTLE_MAP_SURFACE_COFFIN = 0x2e,
    BATTLE_MAP_SURFACE_CROSS_SECTION = 0x3f,
} battle_map_surface_type_e;

enum { BATTLE_MAP_SURFACE_MASK = 0x3f };

/* Selectors accepted by battle_map_tile_get_value and battle_map_tile_set_value.
 * Selector 14 is unsupported; setter selector 15 performs a separate operation
 * whose relationship to tile data is not established. No caller passes
 * selector 1 or 10. */
typedef enum battle_map_tile_data_selector {
    BATTLE_MAP_TILE_DATA_SURFACE_TYPE = 0,
    BATTLE_MAP_TILE_DATA_UNKNOWN_01 = 1,
    BATTLE_MAP_TILE_DATA_HEIGHT = 2,
    BATTLE_MAP_TILE_DATA_HALF_HEIGHT = 3,
    BATTLE_MAP_TILE_DATA_DEPTH = 4,
    BATTLE_MAP_TILE_DATA_SLOPE_TYPE = 5,
    BATTLE_MAP_TILE_DATA_CEILING_DEPTH = 6,
    BATTLE_MAP_TILE_DATA_BLOCKED = 7,
    BATTLE_MAP_TILE_DATA_UNTARGETABLE = 8,
    BATTLE_MAP_TILE_DATA_SHADING = 9,
    BATTLE_MAP_TILE_DATA_UNKNOWN_FLAGS_4_6 = 10,
    BATTLE_MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK = 11,
    BATTLE_MAP_TILE_DATA_MOVE_DESTINATION_MARK = 12,
    BATTLE_MAP_TILE_DATA_ABILITY_RANGE_MARK = 13,
} battle_map_tile_data_selector_e;

/* Depth-bias flag in terrain tile byte0; the low six bits use BATTLE_MAP_SURFACE_MASK. */
enum {
    BATTLE_MAP_TILE_SURFACE_FLAG_DEPTH_BIAS = 0x40,
};

typedef struct battle_map_tile_surface {
    u8 type : 6;       /* battle_map_surface_type_e */
    u8 depth_bias : 1; /* camera depth offset 5 instead of 1 for the cursor, units and shadows on this tile */
    u8 _unused_00_bit7 : 1;
} battle_map_tile_surface_t;

/* Packed mark flags in terrain tile byte 0x05. */
enum {
    BATTLE_MAP_TILE_FLAG_MOVE_DESTINATION = 0x20,
    BATTLE_MAP_TILE_FLAG_ABILITY_RANGE = 0x40,
    BATTLE_MAP_TILE_FLAG_TARGETED = 0x80,
};

/* Tile byte 0x05. Retail reads the depth as a bitfield: its zero test and
 * its value are separate loads (projectile_test_position_for_obstacle). */
typedef struct battle_map_tile_marks {
    u8 ceiling_depth : 5;
    u8 move_destination : 1;
    u8 ability_range : 1;
    u8 targeted : 1;
} battle_map_tile_marks_t;

/* Packed collision flags in terrain tile byte 0x06. */
enum {
    BATTLE_MAP_TILE_FLAG_BLOCKED = 0x01,
    BATTLE_MAP_TILE_FLAG_UNTARGETABLE = 0x02,
    BATTLE_MAP_TILE_COLLISION_MASK = 0x03,
    BATTLE_MAP_TILE_FLAG_CANNOT_STOP = 0x80,
};

/* Terrain shading darkens the unit palette by 0, 4, 8 or 12 per channel. */
typedef enum battle_map_tile_shading {
    BATTLE_MAP_TILE_SHADING_NORMAL = 0,
    BATTLE_MAP_TILE_SHADING_DARK = 1,
    BATTLE_MAP_TILE_SHADING_DARKER = 2,
    BATTLE_MAP_TILE_SHADING_DARKEST = 3,
} battle_map_tile_shading_e;

typedef struct battle_map_tile_flags {
    u8 blocked : 1;
    u8 untargetable : 1;
    u8 shading : 2;
    /* Values 0-5 in the terrain files; copied to the unread trajectory obstacle result. */
    u8 _unknown_00 : 3;
    u8 cannot_stop : 1; /* set by no terrain file */
} battle_map_tile_flags_t;

enum {
    BATTLE_MAP_TILE_HALF_HEIGHT_MASK = 0x1f,
    BATTLE_MAP_TILE_DEPTH_VALUE_MASK = 0x07,
    BATTLE_MAP_TILE_DEPTH_SHIFT = 5,
};

/* Tile byte 0x03: five-bit slope half-height and three-bit depth. Both
 * contribute to the height used by movement and targeting geometry. */
typedef struct battle_map_tile_depth {
    u8 half_height : 5;
    u8 depth : 3;
} battle_map_tile_depth_t;

/* battle_map_tile_t.slope_type shapes. The byte holds the height of each side in two bits (east, west,
 * south and north from bit 0): 0 low, 1 sloping, 2 high. An edge rises towards the side it names;
 * a low corner has one raised corner and a high corner has one lowered corner opposite the one it names. */
typedef enum battle_map_slope_type {
    BATTLE_MAP_SLOPE_LOW_CORNER_SOUTH_EAST = 0x11,
    BATTLE_MAP_SLOPE_LOW_CORNER_SOUTH_WEST = 0x14,
    BATTLE_MAP_SLOPE_EDGE_SOUTH = 0x25,
    BATTLE_MAP_SLOPE_LOW_CORNER_NORTH_EAST = 0x41,
    BATTLE_MAP_SLOPE_LOW_CORNER_NORTH_WEST = 0x44,
    BATTLE_MAP_SLOPE_EDGE_EAST = 0x52,
    BATTLE_MAP_SLOPE_EDGE_WEST = 0x58,
    BATTLE_MAP_SLOPE_HIGH_CORNER_SOUTH_EAST = 0x66,
    BATTLE_MAP_SLOPE_HIGH_CORNER_SOUTH_WEST = 0x69,
    BATTLE_MAP_SLOPE_EDGE_NORTH = 0x85,
    BATTLE_MAP_SLOPE_HIGH_CORNER_NORTH_EAST = 0x96,
    BATTLE_MAP_SLOPE_HIGH_CORNER_NORTH_WEST = 0x99,
} battle_map_slope_type_e;

/* Direction bits shared by the flat and steep nibbles of tile byte 0x07. */
typedef enum battle_map_tile_camera_block_direction {
    BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST = 0x01,
    BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST = 0x02,
    BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST = 0x04,
    BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST = 0x08,
} battle_map_tile_camera_block_direction_e;

enum {
    BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK = 0x0f,
    BATTLE_MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT = 4,
    BATTLE_MAP_TILE_CAMERA_BLOCK_STEEP_MASK = 0xf0,
};

/* Eight-byte terrain record: battle_map_tile_get_pointer indexes
 * (layer * 256 + y * width + x) * 8. The SetTileHeight instruction
 * (EVENT_OPCODE_SET_TILE_HEIGHT) in script_execute_event modifies bytes 0, 2
 * and 6: it replaces the untargetable bit and clears the blocked bit. Its
 * bitfield accesses reproduce that handler's separate surface-byte reload and
 * flag-update order. */
typedef struct battle_map_tile {
    battle_map_tile_surface_t surface;         /* 0x00 */
    u8 _unknown_01;                            /* 0x01; 0 in every terrain file; reached only by tile data selector 1 */
    u8 height;                                 /* 0x02; tile height in whole units */
    battle_map_tile_depth_t depth_half_height; /* 0x03 */
    u8 slope_type;                             /* 0x04; battle_map_slope_type_e: two bits per exit side */
    battle_map_tile_marks_t ceiling_depth_and_marks; /* 0x05; ceiling depth and the transient map marks */
    battle_map_tile_flags_t flags;
    /* 0x07; low nibble blocks camera directions at a flat pitch; high nibble
     * holds the same direction bits for a steep pitch. */
    u8 camera_block_masks;
} battle_map_tile_t;
typedef char battle_map_tile_size_must_be_8[(sizeof(battle_map_tile_t) == 8) ? 1 : -1];

/* selection */
/* Provisional 0xa-byte selected-tile record, filled by
 * battle_map_store_selected_tile_data and copied whole by
 * unit_panel_copy_billboard_out. _unknown_02/_unknown_04 are the two bytes of
 * the per-surface-type main table g_terrain_tile_info_pairs, 25 and 25
 * for all 64 surface types; nothing reads them back. */
typedef struct battle_map_selected_tile {
    s16 surface_type;   /* 0x00 */
    s16 _unknown_02;    /* 0x02 */
    s16 _unknown_04;    /* 0x04 */
    s16 display_height; /* 0x06: half height + height * 2 + depth * 2 */
    s16 depth;          /* 0x08 */
} battle_map_selected_tile_t;

extern battle_map_selected_tile_t g_battle_map_selected_tile_data;

/* background */
/* Map backgrounds use two four-byte runtime color slots. The fourth byte
 * is copied with each CVECTOR but is not an alpha channel; mesh-file RGB
 * triples have a separate three-byte layout. */
typedef enum battle_map_background_gradient_color {
    BATTLE_MAP_BACKGROUND_GRADIENT_TOP = 0,
    BATTLE_MAP_BACKGROUND_GRADIENT_BOTTOM = 1,
    BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT = 2,
} battle_map_background_gradient_color_e;

typedef struct battle_map_background_gradient_colors {
    CVECTOR colors[BATTLE_MAP_BACKGROUND_GRADIENT_COLOR_COUNT];
} battle_map_background_gradient_colors_t;
typedef char battle_map_background_gradient_colors_size_must_be_8[(sizeof(battle_map_background_gradient_colors_t) == 8)
        ? 1
        : -1];

/* The gradient colors of the Background instruction. */
extern battle_map_background_gradient_colors_t g_event_background_colors;

/* event state */
/* Map-destruction handshake between the event interpreter and the frame loop. */
extern s16 g_battle_map_destruction_wait_state;
extern u16 g_battle_map_refresh_pending;

/* The raw weather script variable, before indoor and snow flags apply. */
s32 battle_map_get_weather(void);
void battle_map_wait_for_refresh(void);
/* Thread body: marks the deployment finished, for a battle that skips the deployment screen. */
void battle_map_skip_deployment_thread(void);

/* script lighting */
/* Interpolates the script light transform (g_script_light_* in script.h) each frame. */
void battle_map_light_thread(void);
void battle_map_reset_script_light_transform(void);

/* unreferenced stubs, neutral names */
s32 battle_map_return_zero_1(void);

#endif

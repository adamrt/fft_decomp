#ifndef BATTLE_MAP_H
#define BATTLE_MAP_H

/* Tactical-map engine: resource loading, mesh rendering, lighting and weather.
 * Shared map IDs and terrain records are declared in map.h. */

#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* item and terrain types */
/* Four serialized move-find item/trap entries per map. */
typedef enum map_move_find_item_entry_flags {
    MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR = 0x01,
    MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP = 0x02,
    MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS = 0x04,
    MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE = 0x08,
    MAP_MOVE_FIND_ENTRY_NO_TRAP = 0x10,
    MAP_MOVE_FIND_ENTRY_NO_ITEM = 0x20,
} map_move_find_item_entry_flags_e;

typedef struct map_move_find_item_entry {
    u8 y : 4;
    u8 x : 4;
    u8 flags : 7; /* map_move_find_item_entry_flags_e */
    u8 elevation : 1;
    u8 rare_item_id;
    u8 common_item_id;
} map_move_find_item_entry_t;
typedef char map_move_find_item_entry_size_must_be_4[(sizeof(map_move_find_item_entry_t) == 4) ? 1 : -1];

typedef struct map_move_find_item_data {
    map_move_find_item_entry_t entries[4];
} map_move_find_item_data_t;
typedef char map_move_find_item_data_size_must_be_16[(sizeof(map_move_find_item_data_t) == 16) ? 1 : -1];

/* Terrain chunk: two dimensions followed by two fixed 256-tile layers. */
typedef struct map_terrain_resource {
    u8 width;                   /* 0x0000; map x dimension */
    u8 depth;                   /* 0x0001; map y dimension */
    map_tile_t tiles[2][0x100]; /* 0x0002; tile levels 0 and 1 */
} map_terrain_resource_t;
typedef char map_terrain_resource_size_must_be_4098[sizeof(map_terrain_resource_t) == 4098 ? 1 : -1];

/* weather types */
/*
 * Script variable 0x23, set by event instruction {3C} Weather.  The same
 * strength scale covers rain and snow; the map's 32-bit palette-mod flags says
 * which one is falling.  battle_map_get_effective_weather folds that flag in by
 * adding 3, so callers that compare against it see the snow values.
 */
typedef enum map_weather {
    BATTLE_WEATHER_NONE = 0,
    BATTLE_WEATHER_NONE_ALT = 1,
    BATTLE_WEATHER_RAIN = 2,
    BATTLE_WEATHER_STORM = 3,
    BATTLE_WEATHER_STRONG_STORM = 4,
    BATTLE_WEATHER_SNOW = 5,
    BATTLE_WEATHER_SNOWSTORM = 6,
    BATTLE_WEATHER_STRONG_SNOWSTORM = 7,
} map_weather_e;

/* Added to a rain value to give its snow counterpart. */
enum {
    BATTLE_WEATHER_SNOW_OFFSET = 3,
};

/* Bits of the map 32-bit weather flag value at 0x800b6698 ("weather palette mods"). */
typedef enum map_weather_flags {
    MAP_WEATHER_FLAG_SNOW = 0x01,
    MAP_WEATHER_FLAG_IGNORE_WEATHER = 0x02,
} map_weather_flags_e;

/* The signed palette modifier shares the 32-bit value above its seven low flag bits. */
enum {
    MAP_WEATHER_MODIFIER_SHIFT = 7,
    MAP_WEATHER_LOW_FLAGS_MASK = 0x7f,
};

/* palette cycle types */
/* Eight CLUT color-cycle slots at 0x800a1b98. Each holds a row and a color
 * range to rotate; they are separate from the mesh's 32 texture-animation
 * instructions and their handle table. */
typedef struct map_clut_cycle_state {
    u8 active;
    u8 elapsed_frames;
    u8 frame_duration;
    u8 _padding_03;  /* aligns clut_row */
    u16 clut_row;    /* 0x04; map_clut_row_e */
    u16 first_color; /* 0x06; color index in the row: the colors first..last rotate one place */
    u16 last_color;  /* 0x08; every frame_duration frames, towards last */
} map_clut_cycle_state_t;
typedef char map_clut_cycle_state_size_must_be_10[(sizeof(map_clut_cycle_state_t) == 10) ? 1 : -1];

/* Weather particle positions at 0x800fc358: weather 0x8f (snow) uses all 64,
 * 0x90 (rain) fills two sets of 32. */
typedef struct battle_weather_particle {
    s16 x;
    s16 y;
    s16 z;
    /* g_battle_map_weather_fall_speed, plus the sprite index for 0x8f; rain
     * reuses it as splash state (0/1/2) and the splash set as a countdown. */
    s16 fall_speed;
} battle_weather_particle_t;

/* resource selectors */
typedef enum gns_map_state_comparison {
    GNS_MAP_STATE_EQUAL = 1,
    GNS_MAP_STATE_REQUESTED_LESS_THAN_RECORD = 2,
    GNS_MAP_STATE_REQUESTED_GREATER_THAN_RECORD = 3,
} gns_map_state_comparison_e;

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

/* Private dispatcher operations issued after a texture load, not serialized IDs. */
typedef enum map_resource_apply_command {
    MAP_RESOURCE_APPLY_LIGHTING = 0x32,
    MAP_RESOURCE_APPLY_PALETTES = 0x33,
} map_resource_apply_command_e;

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

/* lighting and weather commands */
/* Commands of battle_map_light_matrix_command. A START queues its STEP in
 * g_battle_map_light_transition_command, which the map draw loop issues every frame. */
typedef enum battle_map_light_matrix_command {
    MAP_LIGHT_MATRIX_COMMAND_SET = 0x5e,
    MAP_LIGHT_MATRIX_COMMAND_SAVE = 0x5f,
    MAP_LIGHT_MATRIX_COMMAND_RESTORE = 0x60,
    MAP_LIGHT_MATRIX_COMMAND_GET_COLOR = 0x61,
    MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT = 0x62,
    MAP_LIGHT_MATRIX_COMMAND_START_BLEND = 0x66, /* towards the given matrices over the duration */
    MAP_LIGHT_MATRIX_COMMAND_STEP_BLEND = 0x67,
    MAP_LIGHT_MATRIX_COMMAND_START_DELTA = 0x68, /* add a frame-scaled delta */
    MAP_LIGHT_MATRIX_COMMAND_STEP_DELTA = 0x69,
} map_light_matrix_command_e;

/* Commands of battle_map_run_weather_command and battle_map_init_weather_particles. */
typedef enum map_weather_command {
    MAP_WEATHER_COMMAND_SNOW = 0x53,
    MAP_WEATHER_COMMAND_CLEAR = 0x54,
    MAP_WEATHER_COMMAND_RAIN = 0x55,
    MAP_WEATHER_COMMAND_LIGHT_RAIN = 0x7c,
    MAP_WEATHER_COMMAND_HEAVY_RAIN = 0x7d,
    MAP_WEATHER_COMMAND_LIGHT_SNOW = 0x7e,
    MAP_WEATHER_COMMAND_HEAVY_SNOW = 0x7f,
    MAP_WEATHER_COMMAND_THUNDER = 0x89,              /* one lightning flash with its sound */
    MAP_WEATHER_COMMAND_LIGHTNING = 0x8a,            /* random lightning from now on */
    MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER = 0x8b, /* issue the commands for the weather script variable */
    MAP_WEATHER_COMMAND_RESTORE_LIGHTING = 0x8c,
    MAP_WEATHER_COMMAND_INIT_SNOW_PARTICLES = 0x8f, /* battle_map_init_weather_particles */
    MAP_WEATHER_COMMAND_INIT_RAIN_PARTICLES = 0x90,
} map_weather_command_e;

/* Stored renderer selector at g_battle_map_weather_effect_mode. The ordinary
 * modes reuse command IDs; 0x96 runs E458's timed particle transform. */
typedef enum map_weather_effect_mode {
    MAP_WEATHER_EFFECT_SNOW = MAP_WEATHER_COMMAND_SNOW,
    MAP_WEATHER_EFFECT_CLEAR = MAP_WEATHER_COMMAND_CLEAR,
    MAP_WEATHER_EFFECT_RAIN = MAP_WEATHER_COMMAND_RAIN,
    MAP_WEATHER_EFFECT_TIMED_PARTICLE_TRANSFORM = 0x96,
} map_weather_effect_mode_e;

/* g_battle_map_lightning_mode. */
typedef enum map_lightning_mode {
    MAP_LIGHTNING_OFF = 0,
    MAP_LIGHTNING_RANDOM = 1,
    MAP_LIGHTNING_ONCE = 2, /* back to OFF when the flash ends */
} map_lightning_mode_e;

/* loading states */
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

/* polygon and lighting commands */
/* Commands of battle_map_polygon_flag_command. */
typedef enum battle_map_polygon_flag_command {
    MAP_POLYGON_FLAG_COMMAND_RESET_ACTIVE = 0x46,    /* copy LIT into LIT_ACTIVE on every polygon */
    MAP_POLYGON_FLAG_COMMAND_CLEAR_ACTIVE = 0x47,    /* clear LIT_ACTIVE everywhere */
    MAP_POLYGON_FLAG_COMMAND_RESTORE_AMBIENT = 0x94, /* repaint the polygons without LIT in the ambient color */
} map_polygon_flag_command_e;

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

/* Commands for the map's background gradient, ambient and darkness colors. */
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

/* Commands of battle_map_configure_effect_data_state: the ordering-table
 * handling of untextured map polygons while an effect draws. */
typedef enum map_effect_data_command {
    MAP_EFFECT_DATA_END = 0x87,
    MAP_EFFECT_DATA_BEGIN = 0x88,
} map_effect_data_command_e;

/* palette and color types */
/* Map color-transition commands: the source color and treatment of RGB offsets.
 * battle_gfx_modify_vram_palette honors modes 5-7; every other value adds the
 * offsets to its supplied colors, including RESTORE. */
typedef enum map_color_mode {
    MAP_COLOR_MODE_CURRENT = 0,           /* the color in transition plus the offsets */
    MAP_COLOR_MODE_CURRENT_HALF = 1,      /* half of it plus the offsets */
    MAP_COLOR_MODE_CURRENT_GRAY = 2,      /* its gray (2R + 3G + B) / 6 plus the offsets */
    MAP_COLOR_MODE_CURRENT_GRAY_HALF = 3, /* half that gray plus the offsets */
    MAP_COLOR_MODE_BASE = 4,              /* the last applied color plus the offsets */
    MAP_COLOR_MODE_BASE_HALF = 5,
    MAP_COLOR_MODE_BASE_GRAY = 6,
    MAP_COLOR_MODE_BASE_GRAY_HALF = 7,
    MAP_COLOR_MODE_RESTORE = 8,      /* map transitions restore the last applied color and ignore offsets */
    MAP_COLOR_MODE_BASE_REPEAT = 9,  /* as BASE, and record the offsets so that the fade alternates with RESTORE */
    MAP_COLOR_MODE_STOP_REPEAT = 10, /* only clear the repeat record */
} map_color_mode_e;

/* The repeat member of the map color transitions and of the CLUT animations. */
typedef enum map_color_repeat {
    MAP_COLOR_REPEAT_OFF = 0,
    MAP_COLOR_REPEAT_TINTED = 1,   /* fading to the recorded offsets; a fade back to the base follows */
    MAP_COLOR_REPEAT_RESTORED = 2, /* fading back to the base; the offsets follow again */
} map_color_repeat_e;

/* A CLUT row of the battle map: one VRAM line of sixteen 16-color CLUTs,
 * at y 0x1e0 through 0x1ed (battle_map_upload_clut_rows uploads a 256x14 image). Index into
 * map_palette_state_t.clut_rows and g_battle_map_palette_modified_colors. */
typedef enum map_clut_row {
    MAP_CLUT_ROW_MAP = 0,
    MAP_CLUT_ROW_MARKED_TILES = 1, /* the map's CLUTs for the polygons of marked tiles */
    MAP_CLUT_ROW_SPRITES = 2,      /* weather particles, deploy portraits, arrow, cursor, shadow, status bubble */
    MAP_CLUT_ROW_UNITS = 3,        /* one CLUT per unit, as in rows 4 to 7 and 10 */
    MAP_CLUT_ROW_UNIT_SHADOWS = 4,
    MAP_CLUT_ROW_ITEM_A = 5, /* the two palettes of the item a unit holds or throws */
    MAP_CLUT_ROW_ITEM_B = 6,
    MAP_CLUT_ROW_ACTION_RESULTS = 7,
    MAP_CLUT_ROW_UNITS_SECOND = 10, /* the second eight palettes of each unit's sheet */
    MAP_CLUT_ROW_WEAPONS = 11,
    MAP_CLUT_ROW_EFFECT = 12,
    MAP_CLUT_ROW_EFFECT_SECOND = 13,
} map_clut_row_e;

enum {
    MAP_CLUT_ROW_COUNT = 14,
    MAP_CLUTS_PER_ROW = CLUT_PALETTE_COUNT,
    MAP_CLUT_COLOR_COUNT = PALETTE_COLOR_COUNT,
    MAP_CLUT_ROW_COLOR_COUNT = MAP_CLUTS_PER_ROW * MAP_CLUT_COLOR_COUNT,
    MAP_CLUT_ROWS_Y = 480, /* VRAM line of MAP_CLUT_ROW_MAP; the rows follow one line apart */
};

/* The 16-bit CLUT value of CLUT clut_index of a map CLUT row: the value of
 * GetClut(clut_index * 16, MAP_CLUT_ROWS_Y + clut_row), as a constant. */
#define MAP_CLUT(clut_row, clut_index) (((MAP_CLUT_ROWS_Y + (clut_row)) << 6) | (clut_index))

/* Eight runtime slots rotate a range of colors of a CLUT row for map texture animations.
 * They are separate from the 32 serialized 0x14-byte animation instructions
 * a map's mesh file carries. */
enum {
    MAP_CLUT_CYCLE_SLOT_COUNT = 8,
    MAP_CLUT_CYCLE_SLOT_BYTE_COUNT = 10,
};

/* A signed five-bit channel delta (-31..31), biased for table indexing. */
enum {
    MAP_PALETTE_DELTA_BIAS = 31,
    MAP_PALETTE_DELTA_COUNT = 63,
    MAP_PALETTE_BLEND_STEP_COUNT = 32,
    MAP_PALETTE_FAST_BLEND_STEP_COUNT = 8,
};

/* Expanded five-bit RGB components and channel deltas biased by 31;
 * each channel occupies one byte, with the transparency bit stored separately. */
typedef struct map_palette_color_components {
    rgb24_t color;
    u8 semi_transparent;
    rgb24_t delta_biased; /* target - current + MAP_PALETTE_DELTA_BIAS */
} map_palette_color_components_t;
typedef char map_palette_color_components_size_must_be_7[(sizeof(map_palette_color_components_t) == 7) ? 1 : -1];

typedef struct map_palette_animation_state {
    u8 active;
    u8 blend_step;    /* 0x01; index into g_battle_map_palette_blend_steps_32/_8 */
    u8 delay_counter; /* 0x02; one blend step each time it reaches mode >> 2 */
    u8 mode;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
} map_palette_animation_state_t;
typedef char map_palette_animation_state_size_must_be_8[(sizeof(map_palette_animation_state_t) == 8) ? 1 : -1];

/* The 0x982-byte runtime state of one CLUT row (map_clut_row_e), based at 0x800995f4:
 * an animation per CLUT, then the 256 colors of the row. Components and
 * packed_colors are runtime state/mirror, not the separate 0x200-byte-per-row
 * upload staging at 0x800e4ea4 (g_battle_map_palette_modified_colors). Palette extraction fills
 * both; 0x80092f98 uploads staging and clears the pending 32-bit value at 0x800995ec. */
typedef struct map_clut_row_state {
    u8 enabled;
    u8 updates_disabled;
    map_palette_animation_state_t animations[MAP_CLUTS_PER_ROW];
    map_palette_color_components_t components[MAP_CLUT_ROW_COLOR_COUNT];
    u16 packed_colors[MAP_CLUT_ROW_COLOR_COUNT];
} map_clut_row_state_t;
typedef char map_clut_row_state_size_must_be_2434[(sizeof(map_clut_row_state_t) == 2434) ? 1 : -1];

/* Palette runtime state block at 0x800995ec: battle_map_update_animations
 * addresses the CLUT rows (0x800995f4) from this base. The upload/move 32-bit values are
 * the pending flags battle_map_upload_clut_rows (0x80092f98) services. */
typedef struct map_palette_state {
    s32 upload_pending;
    s32 move_pending;
    map_clut_row_state_t clut_rows[MAP_CLUT_ROW_COUNT];
} map_palette_state_t;
typedef char map_palette_state_size_must_be_34084[(sizeof(map_palette_state_t) == 34084) ? 1 : -1];

/* resource formats */
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

/* mesh geometry */
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

/* Mesh files begin with 49 byte offsets indexed by map_resource_type_e.
 * The dispatcher visits IDs 0x10..0x30; zero means the chunk is absent.
 * Reserved offsets are zero on the USA disc. MAP000.5 alone has an embedded
 * texture quarter at resource ID 0x13. */
typedef u32 map_mesh_file_header_t[GNS_RESOURCE_TABLE_COUNT];
typedef char map_mesh_file_header_size_must_be_196[(sizeof(map_mesh_file_header_t) == 196) ? 1 : -1];

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

/* resource selection */
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

/* color transitions */
/* Runtime color fades stepped by update_map_animations (0x800912a4):
 * g_battle_map_background_gradient_transition at 0x800a1b10, the ambient light at
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
typedef char map_color_transition_channels_size_must_be_24[(sizeof(map_color_transition_channels_t) == 24) ? 1 : -1];

typedef struct map_color_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
    map_color_transition_channels_t channels;
} map_color_transition_t;
typedef char map_color_transition_size_must_be_32[(sizeof(map_color_transition_t) == 32) ? 1 : -1];

/* The background gradient fade: one channel set per gradient color. colors[]
 * at 0x38 (0x800a1b48/0x800a1b4c) is the applied gradient pair;
 * battle_map_blend_background_gradient_color indexes it from the same base. */
typedef struct map_gradient_transition {
    u8 active;
    u8 phase;
    u8 tick; /* 0x02; frame counter, reset when it reaches period >> 2 */
    u8 period;
    u8 repeat;            /* 0x04; map_color_repeat_e */
    rgb24_t repeat_color; /* 0x05 */
    map_color_transition_channels_t channels[2];
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];
} map_gradient_transition_t;
typedef char map_gradient_transition_size_must_be_64[(sizeof(map_gradient_transition_t) == 64) ? 1 : -1];

/* mesh animation types */
/* 0x98-byte map mesh part records at 0x800fbe00. Each of the rotation,
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
     * by battle_map_append_mesh_geometry (0x800f4dd4). */
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

/* texture animation types */
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

/* Provisional runtime copy of one 0x14-byte map texture animation
 * instruction (32 rows).
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

/* lighting resource types */
/* Lightning flash state at 0x800f79c0 (g_battle_map_lightning_state).
 * battle_map_set_light_matrices fills color_matrices[0] and [2]; the lightning weather commands clear the rotation part
 * of color_matrices[1] and start the 60-frame flash timer. */
typedef struct battle_lightning_state {
    u32 timer;                                                /* 0x00 */
    MATRIX color_matrices[3];                                 /* 0x04 */
    u8 _unused_64[0x20];                                      /* 0x64 */
    CVECTOR base_colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT]; /* 0x84 */
    CVECTOR colors[MAP_BACKGROUND_GRADIENT_COLOR_COUNT];      /* 0x8c */
} battle_lightning_state_t;
typedef char battle_lightning_state_size_must_be_148[(sizeof(battle_lightning_state_t) == 148) ? 1 : -1];

/* resource blocks */
/* Nested records in MAP_RESOURCE_STATE_RECORDS, a mesh-file chunk at slot 0x23. */
typedef enum map_mesh_animation_record_type {
    MAP_MESH_ANIMATION_RECORD_END = 0,
    MAP_MESH_ANIMATION_RECORD_KEYFRAMES = 1,
    MAP_MESH_ANIMATION_RECORD_INSTRUCTIONS = 2,
    MAP_MESH_ANIMATION_RECORD_PART_PROPERTIES = 3,
} map_mesh_animation_record_type_e;

/* Record header parsed by the map-state loader at 0x800f2294: a 32-bit type,
 * an entry count and the packed entries. */
typedef struct map_state_record {
    u32 type;  /* map_mesh_animation_record_type_e */
    u32 count; /* types 1/3: entry count; type 2: packed instruction/state dimensions */
    u8 data[4];
} map_state_record_t;

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

/* resources */
extern map_resource_t* g_battle_map_data_load_buffer;
extern s32 g_battle_map_data_load_complete;
extern s32 g_battle_map_deep_dungeon_load_in_progress;
extern char g_battle_map_directory_path[];
extern u32 g_battle_map_gns_lba[MAP_ID_COUNT]; /* 0 where the map id has no file */
extern u32 g_battle_map_gns_record_cursor;
extern gns_record_t g_battle_map_gns_records[150];
extern s32 g_battle_map_id;
extern map_resource_t* g_battle_map_mesh_load_buffer;
extern s32 g_battle_map_mesh_load_state;
extern s32 g_battle_map_resource_load_state; /* map_resource_load_state_e */
extern u16 g_battle_map_append_state;        /* 0x80165fe0 */

/* 1 starts, 2 polls, >=3 is reset to zero by the outer update. */
extern u16 g_battle_map_reload_state; /* 0x80165fde */

/* GNS records selected for loading and the ones currently loaded, indexed by
 * resource type. */
extern gns_record_t g_battle_map_pending_gns_records[GNS_RESOURCE_TABLE_COUNT];
extern gns_record_t g_battle_map_loaded_gns_records[GNS_RESOURCE_TABLE_COUNT];
extern u8 g_battle_map_gns_dispatch_held;
extern s32 g_battle_map_gns_load_phase; /* map_gns_load_phase_e */
extern s32 g_battle_map_loaded_id;
extern RECT g_battle_map_vram_load_rect;

void map_data_load_complete_thread(void);
s32 battle_map_dispatch_gns_resource(u8 resource_type, map_resource_t* data);
s32 battle_map_start_data_load(void);
s32 battle_map_poll_data_load(void);
void battle_deployment_mark_finished(void);
s32 battle_map_step_init_sequence(s32 map_id, s32 step);
s32 battle_map_load_data(s32 map_id, s32 command);
s32 battle_map_apply_gns_command(s32 phase, map_resource_t* record);
s32 battle_map_dispatch_map_data_command(map_data_command_e command, u32 index, s32 object_state, s32 flag);
s16 battle_map_is_gns_record_matching_state(s32 comparison, s16 record_state, s16 requested_state);
void* battle_map_start_file_load(
    const char* unused_directory_path, const gns_load_info_t* load_info, void* destination, s32 unused_resource_type);
s32 battle_map_init_units_sprites_event_and_music(s32 map_id, s32 step, s32 deployment_running);
s32 battle_map_load_alternate_mesh(s32 palette_mode);

/* terrain and selection */
extern u8 g_battle_map_max_x;
extern u8 g_battle_map_max_y;

/* Battle terrain: two layers of 256 records, indexed
 * (layer * 256 + y * width + x). Callers that walk it with raw byte offsets
 * cast to u8*; the record shape is map_tile_t. */
extern map_tile_t g_battle_map_tile_data[MAP_TILE_COUNT];
extern s32 g_battle_map_lowest_surface_y;
extern map_tile_t* g_battle_map_tile_data_ptr;
extern u8 g_battle_map_tile_depth;
extern u8 g_battle_map_tile_width;

s32 battle_map_calculate_location(const unit_stats_t* unit);
s32 battle_map_get_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer);
map_tile_t* battle_map_get_tile_data_pointer(s32 x, s32 y, u32 layer);
s32 battle_map_set_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer, s32 value);
map_tile_t* battle_map_get_unit_tile_by_runtime_id(u32 runtime_id);
void battle_map_store_selected_tile_coordinates(SVECTOR* destination);
s32 battle_map_calculate_slope_height(const SVECTOR* coords, s32 map_z);
s32 battle_map_resolve_rotated_offset_layer(unit_stats_t*, s32, s32, s32, s32*, s32*, s32*);
void battle_map_store_max_coordinates(SVECTOR* destination);

/* A typed terrain parameter loses the first retail byte-cursor increment. */
void battle_map_copy_xy_coords_and_tile_data(u8* data);
map_tile_t* battle_map_get_tile_data_ptr_from_battle_id(u32 battle_id);
map_tile_t* battle_map_get_unit_tile_from_world_coords(u32 runtime_id);

/* items and traps */
extern map_move_find_item_data_t g_battle_map_move_find_item_data[MAP_ID_COUNT];
extern map_move_find_item_data_t g_battle_current_map_move_find_item_data;
extern move_found_item_t g_battle_map_rare_item_result;

void battle_map_load_move_find_item_data(map_move_find_item_data_t* map_data);
move_found_item_t* battle_map_determine_rare_common_item(unit_stats_t* unit);
move_find_result_data_t* battle_map_get_move_find_result(s32 x, u8 y, u8 layer);
gns_record_t* battle_map_load_gns_and_move_find_items(s32 map_id);

/* palettes */
extern u16 g_battle_map_palette_modified_colors[][MAP_CLUT_ROW_COLOR_COUNT]; /* [map_clut_row_e][color] */
extern map_clut_cycle_state_t g_battle_map_clut_cycle_states[];
extern map_palette_state_t g_battle_map_palette_state;
extern clut_t g_battle_map_palette_animation_frames;
extern u8 g_battle_map_palette_pending;
extern u8 g_battle_map_pending_palette_data[0x200];

void battle_map_init_palette_state(void);
s32 battle_map_stop_clut_cycle(s32 handle);
void battle_map_store_modified_palette_colors(s32 clut_row, s32 clut_index, s32 store_all);
void battle_map_modify_palette(
    s32 command, s32 amount, s32 clut_row, s32 clut_index, s32 all_colors, s16 red, s16 green, s16 blue);
void battle_map_upload_clut_rows(void);
void battle_map_update_all_palettes_from_color_data(s32 mode, const clut_t color_data);
void battle_map_update_palette_colors_by_index(s32 mode, s32 clut_index, const palette_t* color_data);
void battle_map_update_and_store_palette_colors(
    s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors);
void battle_map_load_palette_data(const palette_t* palettes, s32 clut_row, s32 clut_index, s32 load_all);
void battle_map_update_palette_colors(s32 mode, s32 clut_row, s32 clut_index, s32 update_all, const palette_t* colors);
s32 battle_map_start_clut_cycle(s32 frame_duration, s32 clut_row, s32 first_color, s32 last_color);
void battle_map_update_palette_from_other(const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors,
    s16 red, s16 green, s16 blue, s32 final_value);

/* lighting and colors */
extern rgb24_t g_battle_map_background_color;
extern rgb24_t g_battle_map_background_color_backup;
extern MATRIX g_battle_map_light_color_matrix;
extern MATRIX g_battle_map_light_matrix;
extern SVECTOR g_battle_map_light_rotation;
extern s32 g_battle_map_back_color_red;
extern s32 g_battle_map_back_color_green;
extern s32 g_battle_map_back_color_blue;
extern u8 g_battle_map_back_color_disabled;
extern MATRIX g_battle_map_light_rotation_matrix;
extern battle_render_buffer_t* g_battle_map_ambient_restored_buffer;

/* The applied background gradient pair at 0x801251c8. The copies at
 * 0x800e81d0, 0x800e8228 and 0x800e827c move each color as an independent
 * unaligned 32-bit value (lwl/lwr + swl/swr). */
extern map_background_gradient_colors_t g_battle_map_background_gradient_colors;
extern CVECTOR g_battle_map_ambient_light_color;
extern CVECTOR g_battle_map_darkness_color;
extern map_gradient_transition_t g_battle_map_background_gradient_transition;
extern map_color_transition_t g_battle_map_ambient_light_transition;
extern map_color_transition_t g_battle_map_darkness_transition;
extern rgb24_t g_battle_map_ambient_polygon_color;
extern CVECTOR g_battle_map_cached_back_color;
extern CVECTOR g_battle_map_current_darkness_color;
extern u16 g_battle_map_light_transition_command;               /* pending per-frame light command, 0 when idle */
extern u16 g_battle_map_light_transition_duration;              /* transition duration in frames */
extern u16 g_battle_map_light_transition_frame;                 /* transition frame counter */
extern MATRIX g_battle_map_light_transition_start_color_matrix; /* transition start color matrix */
extern MATRIX g_battle_map_light_transition_start_light_matrix; /* transition start light matrix */
extern MATRIX
    g_battle_map_light_transition_target_color_matrix; /* transition target (0x66) or delta (0x68) color matrix */
extern MATRIX
    g_battle_map_light_transition_target_light_matrix; /* transition target (0x66) or delta (0x68) light matrix */
extern u8 g_battle_map_lighting_pending;
extern MATRIX g_battle_map_local_light_matrix;
extern u8 g_battle_map_pending_lighting_data[0x2d];
extern s32 g_battle_map_saved_back_color_blue;
extern s32 g_battle_map_saved_back_color_green;
extern s32 g_battle_map_saved_back_color_red;
extern CVECTOR g_battle_map_saved_background_gradient_first;
extern CVECTOR g_battle_map_saved_background_gradient_second;
extern MATRIX g_battle_map_saved_light_color_matrix; /* saved color matrix (command 0x5f/0x60) */
extern MATRIX g_battle_map_saved_light_matrix;       /* saved light matrix (command 0x5f/0x60) */

void battle_map_init_ambient_light(void);
void battle_map_init_background_gradient(void);
void battle_map_start_darkness_blend(s32 first, s32 second, s16 third, s16 fourth, s16 fifth);
void battle_map_color_field(s32 first, s32 second, s16 red, s16 green, s16 blue);

/* Only SET/GET define a result (stored RGB pointer, or null for SET while
 * disabled). Other command paths leave the target return register unspecified. */
s32* battle_map_control_gte_background_color(s32 command, s32 red, s32 green, s32 blue);
void battle_map_copy_background_color(void);
void battle_map_restore_background_color(void);
rgb24_t* battle_map_get_background_color(void);
void battle_map_set_background_color(s32 red, s32 green, s32 blue);
void battle_map_adjust_background_color(s16 red_delta, s16 green_delta, s16 blue_delta);
MATRIX* battle_map_light_matrix_command(map_light_matrix_command_e command, MATRIX* color, MATRIX* light, s32 duration);
void battle_map_modify_background_gradient(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_background_gradient(s32 frame_duration, const map_background_gradient_colors_t* colors);
void battle_map_start_ambient_light_blend(s32 mode, s32 frame_duration, s16 red, s16 green, s16 blue);
void battle_map_set_darkness_color(s32 frame_duration, const CVECTOR* color);
void battle_map_transition_and_store_background_gradient(
    s32 frame_duration, const map_background_gradient_colors_t* colors);

/* Map data, palette and mesh interfaces implemented in the battle overlay. */
void battle_map_init_darkness(void);
void* battle_map_light_state_command(map_light_command_e command, void* data);

/* mesh animation */
extern map_mesh_part_t g_battle_map_mesh_parts[9];
extern map_mesh_keyframe_t g_battle_map_mesh_animation_keyframes[];
extern map_mesh_instructions_t g_battle_map_mesh_animation_instructions[9];
extern s32 g_battle_map_mesh_part_animation_states[];
extern const map_mesh_geometry_t* g_battle_map_mesh_part_geometry[9];

void battle_map_update_animations(void);
void battle_map_freeze(void);
void battle_map_unfreeze(void);
s32 battle_map_command_get_3d_object_state(s32 value);
s32 battle_map_command_set_3d_object_state(s32 value_a, s32 value_b);

/* texture animation */
extern s32 g_battle_map_texture_animation_cycle_handles[MAP_TEXTURE_ANIMATION_COUNT];
extern map_texture_animation_t g_battle_map_texture_animations[MAP_TEXTURE_ANIMATION_COUNT];
extern DR_MOVE g_battle_map_texture_animation_moves[MAP_TEXTURE_ANIMATION_COUNT];
extern u8 g_battle_map_saved_texture_animation_modes[];
extern u8 g_battle_map_texture_pending;
extern u8 g_battle_map_texture_quarter_0_pending;
extern u8 g_battle_map_texture_quarter_1_pending;
extern u8 g_battle_map_texture_quarter_2_pending;
extern u8 g_battle_map_texture_quarter_3_pending;

s32 battle_map_command_get_texture_animation_active(s32 value);
s32 battle_map_command_start_texture_animation(s32 value_a, s32 value_b);

/* mesh rendering */
extern gns_record_t g_battle_map_selected_mesh_record;
extern battle_render_buffer_t* g_battle_map_mesh_last_render_buffer;
extern s32 g_battle_map_textured_quad_count;
extern s32 g_battle_map_textured_quad_render_record_count;
extern s32 g_battle_map_textured_triangle_count;
extern s32 g_battle_map_textured_triangle_render_record_count;
extern s32 g_battle_map_tilt_target;
extern s32 g_battle_map_untextured_quad_count;
extern s32 g_battle_map_untextured_quad_render_record_count;
extern s32 g_battle_map_untextured_triangle_count;
extern s32 g_battle_map_untextured_triangle_render_record_count;
extern s32 g_battle_map_zoom_target;
extern VECTOR g_battle_map_camera_zoom;
extern MATRIX g_battle_map_camera_matrix;

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
extern map_mesh_triangle_positions_t g_battle_map_textured_triangle_positions[];
extern map_mesh_quad_positions_t g_battle_map_textured_quad_positions[];
extern map_mesh_triangle_positions_t g_battle_map_untextured_triangle_positions[];
extern map_mesh_quad_positions_t g_battle_map_untextured_quad_positions[];
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

/* weather */
extern palette_t g_battle_map_weather_particle_palette;
extern s16 g_battle_map_weather_effect_mode; /* map_weather_effect_mode_e */
extern s32 g_battle_map_command_0x96_frame;
extern s32 g_battle_map_command_0x96_duration;
extern u8 g_battle_map_weather_primary_speed;
extern u8 g_battle_map_rain_secondary_fall_speed;
extern battle_weather_particle_t g_battle_map_rain_splash_particles[32];
extern RECT g_battle_map_weather_drop_uv_rects[3];
extern RECT g_battle_map_weather_splash_uv_rects[4];
extern battle_weather_particle_t g_battle_map_weather_particles[64];
extern battle_lightning_state_t g_battle_map_lightning_state;
extern SVECTOR g_battle_map_command_0x96_rotation;
extern s32 g_battle_map_lightning_mode;
extern u8 g_battle_map_weather_fall_speed;

/* Map weather flags: bit 1 suppresses weather, bit 0 marks snow. */
extern s32 g_battle_map_weather_flags;
extern MATRIX g_battle_map_weather_matrix;
extern SVECTOR g_battle_map_weather_rotation;
extern VECTOR g_battle_map_weather_scale;
extern s32 g_battle_map_weather_sprite_scale;
extern VECTOR g_battle_map_weather_translation;

s32 battle_map_get_effective_weather(void);
void battle_map_set_command_0x96_duration(s32 id, s32 value);
void battle_map_draw_mesh_and_weather(MATRIX* camera_matrix);
s32 battle_map_stop_clut_cycles_and_weather_sfx(void);
void battle_map_run_weather_command(map_weather_command_e command);

#endif

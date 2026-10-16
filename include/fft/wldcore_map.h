#ifndef WLDCORE_MAP_H
#define WLDCORE_MAP_H

/* WLDCORE's world map: the tile atlas and its projection, the camera, scrolling, map dots and the player's marker,
 * routes and route travel, locations and random encounters. */

#include "fft/gfx.h"
#include "fft/wldcore_menu_level.h"
#include "fft/world_sprite.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* tiles */
/* world_map_projected_tile_t.flags. */
typedef enum world_map_tile_flag {
    WORLD_MAP_TILE_TPAGE_MASK = 0x0ff,
    WORLD_MAP_TILE_OFF_SCREEN = 0x100, /* set by the projection pass */
    WORLD_MAP_TILE_EMPTY = 0x200,      /* the atlas has no tile here */
} world_map_tile_flag_e;

/* Provisional: an entry of the projected-tile table (g_world_map_projected_tiles), filled by
 * world_map_init_tile_atlas and projected by the RotTrans pass of world_map_project_tiles. */
typedef struct world_map_projected_tile {
    s32 flags;    /* 0x00; world_map_tile_flag_e */
    s32 texture;  /* 0x04; texture rectangle packed u << 24 | v << 16 | w << 8 | h, unpacked into the poly UVs */
    s32 map_x;    /* 0x08; tile x (-256 + widths), RotTrans vx */
    s32 map_y;    /* 0x0c; tile y (-192 + heights), RotTrans vy */
    s32 screen_x; /* 0x10; projected x, culled against the 0x121 screen span */
    s32 screen_y; /* 0x14; projected y, culled against 0x111 */
} world_map_projected_tile_t;
typedef char wldcore_projected_entry_size_must_be_24[sizeof(world_map_projected_tile_t) == 24 ? 1 : -1];

extern s32* g_world_map_tile_descriptors;
extern world_map_projected_tile_t g_world_map_projected_tiles[];
void world_map_load_texture(void);
void world_map_init_tile_atlas(void);
void world_map_project_tiles(void);

/* camera */
/* Map camera block: translation, scale and rotation are passed together. */
typedef struct world_map_projection_view {
    VECTOR origin;    /* 0x00 */
    VECTOR scale;     /* 0x10 */
    SVECTOR rotation; /* 0x20 */
} world_map_projection_view_t;
typedef char wldcore_map_projection_view_size_must_be_40[sizeof(world_map_projection_view_t) == 40 ? 1 : -1];

/* world_map_pan_t.flags. */
typedef enum world_map_pan_flag {
    WORLD_MAP_PAN_ACTIVE = 0x01,
    WORLD_MAP_PAN_ZOOMED_OUT = 0x02, /* g_world_map_zoom: the wide view, or the move to it */
} world_map_pan_flag_e;

/* Wrapped projection move: the origin and final target are copied as VECTORs. */
typedef struct world_map_pan {
    s32 flags; /* 0x00; world_map_pan_flag_e */
    s32 distance;
    s32 progress;
    VECTOR origin;
    VECTOR target;
} world_map_pan_t;
typedef char wldcore_projection_motion_size_must_be_44[sizeof(world_map_pan_t) == 44 ? 1 : -1];

/* The zoom between the normal and the wide view (g_world_map_zoom). */
typedef struct world_map_zoom {
    s32 flags;
    s32 frame;
    s32 duration;
    VECTOR origin;
} world_map_zoom_t;
typedef char wldcore_zoom_motion_size_must_be_28[sizeof(world_map_zoom_t) == 28 ? 1 : -1];

extern world_map_projection_view_t g_world_map_projection_view;
extern VECTOR g_world_map_projection_origin;
extern world_map_pan_t g_world_map_pan;
/* Provisional: flags bit 0 runs the zoom, bit 1 selects the zoomed-out direction. */
extern world_map_zoom_t g_world_map_zoom;
void world_map_update_camera(void);
s32 world_map_step_pan(void);
void world_map_step_focus_level(menu_stack_record_t* entry);
s32 world_map_start_pan_if_outside(const point32_t* point, const SVECTOR* coordinates);
s32 world_map_is_outside_pan_bounds(const point32_t* point);

/* scrolling */
/* The range of the map scroll position. */
typedef enum world_map_scroll_limits {
    WORLD_MAP_SCROLL_MIN_X = -116,
    WORLD_MAP_SCROLL_MAX_X = 128,
    WORLD_MAP_SCROLL_MIN_Y = -64,
    WORLD_MAP_SCROLL_MAX_Y = 80
} world_map_scroll_limits_e;

/* Provisional: one axis of the accelerating map scroll stepped by
 * world_map_update_scroll_axis: the fixed-point accumulator, its
 * signed limit, the acceleration and decay steps, and the exported whole-unit
 * speed. */
typedef struct world_map_scroll_axis {
    s32 value;
    s32 limit;
    s32 step;
    s32 decay;
    s32 output; /* 0x10; value >> 8, truncated toward zero */
} world_map_scroll_axis_t;

/* The horizontal and vertical map scroll axes. */
extern world_map_scroll_axis_t g_world_map_scroll_x;
extern world_map_scroll_axis_t g_world_map_scroll_y;
void world_map_update_scroll_axis(world_map_scroll_axis_t* axis, s32 direction);
void world_map_apply_clamped_scroll(point32_t* amount, VECTOR* position);
void world_map_apply_clamped_horizontal_scroll(s32* amount, s32* position);
void world_map_apply_clamped_vertical_scroll(point32_t* amount, VECTOR* position);

/* map dots and the marker */
/* A world map dot (g_world_map_dots); the player's marker is one too (world_map_projection_state_t.marker). */
typedef struct world_map_dot {
    s32 kind;
    s32 sub_kind;
    SVECTOR position; /* 0x08; world-map position, RotTrans input */
    /* 0x10; enqueued whole for the animated display-object renderer. Its palette
     * is 0, 2 or 3 for the player's marker, from EVENT_SCRIPT_VAR_TOWN_BACKGROUND. */
    display_object_animated_t render;
} world_map_dot_t;
typedef char wldcore_map_dot_size_must_be_52[sizeof(world_map_dot_t) == 52 ? 1 : -1];

/* Packed eight-byte source row used by world_map_init_dots.
 * The low 16 bits of the first 32-bit value hold x; its high bytes select the animation. */
typedef struct world_map_dot_source {
    s32 position_and_kind;
    s32 y;
} world_map_dot_source_t;
typedef char wldcore_map_dot_source_size_must_be_8[sizeof(world_map_dot_source_t) == 8 ? 1 : -1];

/* world_map_projection_state_t.flags: work pending for
 * world_map_update_camera, and the marker's sprite set. */
typedef enum world_map_projection_flag {
    WORLD_MAP_PROJECTION_UPDATE_POSITION = 0x01,
    WORLD_MAP_PROJECTION_UPDATE_DIRECTION = 0x02,
    WORLD_MAP_PROJECTION_ALTERNATE_DIRECTION_SET = 0x04, /* set while the marker travels a route */
} world_map_projection_flag_e;

/* The world map camera state (g_world_map_projection_state): two control fields followed by the player's own map
 * dot. The marker at +8 makes the views of this object agree field for field: the camera update writes the dot's
 * render sequence and screen position, and the marker init writes its kind, position and flags. For the player's
 * marker, kind is the current location id and sub_kind the location picture id. */
typedef struct world_map_projection_state {
    s32 flags; /* 0x00; world_map_projection_flag_e */
    s32 angle;
    world_map_dot_t marker;
} world_map_projection_state_t;

extern world_map_dot_source_t* g_world_map_dot_sources;
extern s32 g_world_map_dot_count;
extern world_map_dot_t g_world_map_dots[];
extern world_map_projection_state_t g_world_map_projection_state;
extern u8 g_world_map_dot_pulse_direction;
extern u8 g_world_map_dot_pulse_phase;
extern s32 g_world_map_window_index;
void world_map_init_dots(void);
void world_map_init_marker_and_camera(void);
void world_map_project_dots(void);
void world_map_pulse_dots(void);
s32 world_map_find_dot_at(point32_t point);
s32 world_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y);
/* The location whose name label is drawn over its map dot. */
extern s32 g_location_selected;
void location_draw_label(GsOT* ot);

/* routes */
/* World-map routes: g_world_map_routes and the flags from
 * EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST. */
enum { WORLD_MAP_ROUTE_COUNT = 48 };

enum { TRAVEL_PATH_CAPACITY = 16 };

/* A path of route legs, as the route search finds it. */
typedef struct travel_path {
    s32 routes[TRAVEL_PATH_CAPACITY];
} travel_path_t;
typedef char wldcore_route_path_size_must_be_64[sizeof(travel_path_t) == 64 ? 1 : -1];

/* A path entry packs the route id in its low byte and the direction in bit 8. */
enum {
    TRAVEL_PATH_ROUTE_MASK = 0xff,
    TRAVEL_PATH_REVERSED = 0x100,
};

/* A travel point: packed x/y, a 32-bit heading, and segment distance. */
typedef struct world_map_route_point {
    u32 position;
    u32 heading;
    s32 distance;
} world_map_route_point_t;

/* A route header followed by count travel points. */
typedef struct world_map_route {
    u8 count;
    u8 start_location;
    u8 end_location;
    u8 cost;
    world_map_route_point_t points[0]; /* 0x04; variable length */
} world_map_route_t;
typedef char wldcore_route_record_size_must_be_4[sizeof(world_map_route_t) == 4 ? 1 : -1];

/* world_map_path_animation_t.flags. */
typedef enum world_map_path_animation_flag {
    WORLD_MAP_PATH_ANIMATION_RUNNING = 0x01,  /* the segment timer runs */
    WORLD_MAP_PATH_ANIMATION_REVERSED = 0x02, /* the route is shown end to start */
} world_map_path_animation_flag_e;

/* The path animation (g_world_map_path_animation): a route drawn segment by segment. route_id is the route
 * (offset by 0x22c, its route-visible script variable); each segment is one two-point step of the route polyline that
 * follows the segment list, drawn over its length / 32 frames, and segment i runs from point 2i to point 2i + 2. */
typedef struct world_map_path_animation {
    u16 flags;              /* 0x00; world_map_path_animation_flag_e */
    s16 route_id;           /* 0x02; route; + 0x22c is its route-visible script variable */
    s16 segment_count;      /* 0x04 */
    s16 segment_index;      /* 0x06 */
    s16 segment_frames[16]; /* 0x08; frames each segment takes to unroll */
    s16 frame_timer;        /* 0x28 */
    s16 point_count;        /* 0x2a */
    s16 coords[1];          /* 0x2c; packed (x, y) pairs */
} world_map_path_animation_t;

/* The route records by route id: a pointer cell to the WORLD_MAP_ROUTE_COUNT record pointers. */
extern world_map_route_t** g_world_map_routes;
/* The route polylines by route id, as g_world_map_routes: each is a packed s16 count followed by that many
 * (x, y) pairs. */
extern u16** g_world_map_route_polylines;
extern s32 g_travel_search_depth;
extern s32 g_travel_search_cost;
extern travel_path_t g_travel_search_path;
extern world_map_path_animation_t g_world_map_path_animation;
void world_map_draw_routes(GsOT* ot);
void world_map_prepare_path_animation(s32 from, s32 to);
void world_map_draw_path_animation(GsOT* ot);

/* locations and route travel */
/* Provisional: four-byte per-location records reached through g_location_records. */
typedef struct location_record {
    u8 _unused_00[2];
    u8 picture; /* 0x02; location picture set + 1, 0 for none */
    u8 is_town; /* 0x03; 1 offers the town entries TEXT_ID_BAR to _SOLDIER_OFFICE in the location menu */
} location_record_t;

/* travel_t.flags. */
typedef enum travel_flag {
    TRAVEL_ACTIVE = 0x01,   /* a path was found and is being traveled */
    TRAVEL_REVERSED = 0x02, /* the current leg runs its route end to start */
} travel_flag_e;

/* Provisional: the route travel state (g_travel), filled by the route search
 * travel_find_best_path, staged per route leg by travel_begin_segment and stepped by
 * travel_step. The vertices are SVECTORs: their 2-byte alignment is what makes
 * `previous_position = position` take GCC's unaligned block-move path (lwl/lwr, swl/swr) that the target emits. */
typedef struct travel {
    s32 flags;                 /* 0x00; travel_flag_e */
    s32 route_id;              /* 0x04; route & 0xff, index into g_world_map_routes */
    s32 segment_index;         /* 0x08; current segment of the leg, the step for begin_route_segment */
    s32 segment_count;         /* 0x0c; travel_begin_segment result (record count - 1) */
    s32 segment_frame;         /* 0x10; frames into the segment, interpolation numerator */
    s32 segment_frames;        /* 0x14; point distance * 2, interpolation denominator */
    travel_path_t route_path;  /* 0x18; best path found */
    s32 route_index;           /* 0x58; route_path.routes index of the current leg */
    s32 best_path_length;      /* 0x5c */
    s32 best_cost;             /* 0x60; reset to 99999 */
    s32 best_length_bound;     /* 0x64; reset to 99999 */
    s32 leg_start_location;    /* 0x68; route record start (end when reversed) */
    s32 leg_end_location;      /* 0x6c; becomes the current location and script variable 0x31 on arrival */
    s32 heading;               /* 0x70; route point heading (+0x800 when reversed), copied to the marker angle */
    s32 menu_level;            /* 0x74; parent menu depth */
    SVECTOR previous_position; /* 0x78 */
    SVECTOR position;          /* 0x80 */
    SVECTOR target_position;   /* 0x88 */
    s32 arrival_location;      /* 0x90; arrival location id + 1 */
} travel_t;
typedef char wldcore_route_travel_size_must_be_148[sizeof(travel_t) == 148 ? 1 : -1];

extern location_record_t* g_location_records;
extern travel_t g_travel;
extern s32 g_location_label_shade;
extern u32* g_location_picture_frame_tim;
/* Per location choice, its offset from EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST: a set variable hides the
 * choice from the location menu (location_build_menu_entries). */
extern u8 g_location_choice_hidden_offsets[];
/* Indexed so the store in travel_start stays behind the
 * window-flag store (MEM_IN_STRUCT_P). */
extern s32 g_travel_next_map_id[];
s32 travel_start(s32 from_location, s32 to_location);
s32 location_build_menu_entries(s32 location_id, s32* out);
void travel_step(void);

/* random encounters */
/* One random-encounter tier: the story-progress threshold that selects it,
 * the chance an encounter happens (a roll of 0..99 at or below it), and the
 * high-bit-first mask of which of the entry's eight ENTDs may be drawn. */
typedef struct random_battle_tier {
    u8 progress_threshold; /* 0x00 */
    u8 encounter_chance;   /* 0x01 */
    u8 encounter_mask;     /* 0x02 */
} random_battle_tier_t;

/* One road into a world-map location. Three per location, 19 locations
 * (ids 24..42), reached through the pointer cell g_random_battle_roads. */
typedef struct random_battle_road {
    u8 route;                      /* 0x00 */
    u8 deployment_squad_id;        /* 0x01: deployment zone id = this + 0x200 */
    random_battle_tier_t tiers[4]; /* 0x02 */
    u8 entd_ids[8];                /* 0x0e */
    u8 battle_map_id;              /* 0x16 */
    u8 script_variable_id;         /* 0x17 */
} random_battle_road_t;

typedef struct deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 entd_ids[8];
} deep_dungeon_battle_t;

extern random_battle_road_t (*g_random_battle_roads)[3];
extern s32 g_random_battle_entd_id;
extern s32 g_random_battle_map_id;
extern s32 g_random_battle_squad_id;
extern deep_dungeon_battle_t* g_deep_dungeon_battles;

#endif

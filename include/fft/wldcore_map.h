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
/* wldcore_projected_entry_t.flags. */
typedef enum wldcore_map_tile_flag {
    WLDCORE_MAP_TILE_TPAGE_MASK = 0x0ff,
    WLDCORE_MAP_TILE_OFF_SCREEN = 0x100, /* set by the projection pass */
    WLDCORE_MAP_TILE_EMPTY = 0x200,      /* the atlas has no tile here */
} wldcore_map_tile_flag_e;

/* Provisional: an entry of the projected-tile table (g_wldcore_map_projected_tiles), filled by
 * wldcore_map_init_tile_atlas and projected by the RotTrans pass of wldcore_map_project_and_cull_tiles. */
typedef struct wldcore_projected_entry {
    s32 flags;    /* 0x00; wldcore_map_tile_flag_e */
    s32 texture;  /* 0x04; texture rectangle packed u << 24 | v << 16 | w << 8 | h, unpacked into the poly UVs */
    s32 map_x;    /* 0x08; tile x (-256 + widths), RotTrans vx */
    s32 map_y;    /* 0x0c; tile y (-192 + heights), RotTrans vy */
    s32 screen_x; /* 0x10; projected x, culled against the 0x121 screen span */
    s32 screen_y; /* 0x14; projected y, culled against 0x111 */
} wldcore_projected_entry_t;
typedef char wldcore_projected_entry_size_must_be_24[sizeof(wldcore_projected_entry_t) == 24 ? 1 : -1];

extern s32* g_wldcore_map_tile_descriptors;
extern wldcore_projected_entry_t g_wldcore_map_projected_tiles[];
void wldcore_map_load_texture(void);
void wldcore_map_init_tile_atlas(void);
void wldcore_map_project_and_cull_tiles(void);

/* camera */
/* Map camera block: translation, scale and rotation are passed together. */
typedef struct wldcore_map_projection_view {
    VECTOR origin;    /* 0x00 */
    VECTOR scale;     /* 0x10 */
    SVECTOR rotation; /* 0x20 */
} wldcore_map_projection_view_t;
typedef char wldcore_map_projection_view_size_must_be_40[sizeof(wldcore_map_projection_view_t) == 40 ? 1 : -1];

/* wldcore_projection_motion_t.flags. */
typedef enum wldcore_projection_motion_flag {
    WLDCORE_PROJECTION_MOTION_ACTIVE = 0x01,
    WLDCORE_PROJECTION_MOTION_ZOOMED_OUT = 0x02, /* g_wldcore_map_zoom_motion: the wide view, or the move to it */
} wldcore_projection_motion_flag_e;

/* Wrapped projection move: the origin and final target are copied as VECTORs. */
typedef struct wldcore_projection_motion {
    s32 flags; /* 0x00; wldcore_projection_motion_flag_e */
    s32 distance;
    s32 progress;
    VECTOR origin;
    VECTOR target;
} wldcore_projection_motion_t;
typedef char wldcore_projection_motion_size_must_be_44[sizeof(wldcore_projection_motion_t) == 44 ? 1 : -1];

/* The zoom between the normal and the wide view (g_wldcore_map_zoom_motion). */
typedef struct wldcore_zoom_motion {
    s32 flags;
    s32 frame;
    s32 duration;
    VECTOR origin;
} wldcore_zoom_motion_t;
typedef char wldcore_zoom_motion_size_must_be_28[sizeof(wldcore_zoom_motion_t) == 28 ? 1 : -1];

extern wldcore_map_projection_view_t g_wldcore_map_projection_view;
extern VECTOR g_wldcore_map_projection_origin;
extern SVECTOR g_wldcore_map_projection_rotation;
extern VECTOR g_wldcore_map_projection_scale;
extern wldcore_projection_motion_t g_wldcore_map_projection_motion;
/* Provisional: flags bit 0 runs the zoom, bit 1 selects the zoomed-out direction. */
extern wldcore_zoom_motion_t g_wldcore_map_zoom_motion;
void wldcore_map_update_camera_direction_and_projection(void);
s32 wldcore_map_step_projection_motion(void);
void wldcore_map_step_projection_motion_then_pop_level(wldcore_menu_stack_record_t* entry);
s32 wldcore_map_start_projection_motion_if_outside_bounds(const point32_t* point, const SVECTOR* coordinates);
s32 wldcore_map_is_point_outside_projection_bounds(const point32_t* point);

/* scrolling */
/* The range of the map scroll position. */
typedef enum wldcore_scroll_limits {
    WLDCORE_SCROLL_MIN_X = -116,
    WLDCORE_SCROLL_MAX_X = 128,
    WLDCORE_SCROLL_MIN_Y = -64,
    WLDCORE_SCROLL_MAX_Y = 80
} wldcore_map_scroll_limits_e;

/* Provisional: one axis of the accelerating map scroll stepped by
 * wldcore_map_update_scroll_axis_speed: the fixed-point accumulator, its
 * signed limit, the acceleration and decay steps, and the exported whole-unit
 * speed. */
typedef struct wldcore_axis_state {
    s32 value;
    s32 limit;
    s32 step;
    s32 decay;
    s32 output; /* 0x10; value >> 8, truncated toward zero */
} wldcore_axis_state_t;

/* The horizontal and vertical map scroll axes. */
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_x;
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_y;
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction);
void wldcore_map_apply_clamped_scroll(point32_t* amount, VECTOR* position);
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position);
void wldcore_map_apply_clamped_vertical_scroll(point32_t* amount, VECTOR* position);

/* map dots and the marker */
/* A world map dot (g_wldcore_map_dots); the player's marker is one too (wldcore_projection_state_t.marker). */
typedef struct wldcore_map_dot {
    s32 kind;
    s32 sub_kind;
    SVECTOR position; /* 0x08; world-map position, RotTrans input */
    /* 0x10; enqueued whole for the animated display-object renderer. Its palette
     * is 0, 2 or 3 for the player's marker, from EVENT_SCRIPT_VAR_TOWN_BACKGROUND. */
    world_animated_render_record_t render;
} wldcore_map_dot_t;
typedef char wldcore_map_dot_size_must_be_52[sizeof(wldcore_map_dot_t) == 52 ? 1 : -1];

/* Packed eight-byte source row used by wldcore_map_init_dots.
 * The low 16 bits of the first 32-bit value hold x; its high bytes select the animation. */
typedef struct wldcore_map_dot_source {
    s32 position_and_kind;
    s32 y;
} wldcore_map_dot_source_t;
typedef char wldcore_map_dot_source_size_must_be_8[sizeof(wldcore_map_dot_source_t) == 8 ? 1 : -1];

/* wldcore_projection_state_t.flags: work pending for
 * wldcore_map_update_camera_direction_and_projection, and the marker's sprite set. */
typedef enum wldcore_projection_flags {
    WLDCORE_PROJECTION_FLAG_UPDATE_POSITION = 0x01,
    WLDCORE_PROJECTION_FLAG_UPDATE_DIRECTION = 0x02,
    WLDCORE_PROJECTION_FLAG_ALTERNATE_DIRECTION_SET = 0x04, /* set while the marker travels a route */
} wldcore_projection_flags_e;

/* The world map camera state (g_wldcore_map_projection_state): two control fields followed by the player's own map
 * dot. The marker at +8 makes the views of this object agree field for field: the camera update writes the dot's
 * render sequence and screen position, and the marker init writes its kind, position and flags. For the player's
 * marker, kind is the current location id and sub_kind the proposition picture id. */
typedef struct wldcore_projection_state {
    s32 flags; /* 0x00; wldcore_projection_flags_e */
    s32 angle;
    wldcore_map_dot_t marker;
} wldcore_projection_state_t;

extern wldcore_map_dot_source_t* g_wldcore_map_dot_sources;
extern s32 g_wldcore_map_dot_count;
extern wldcore_map_dot_t g_wldcore_map_dots[];
extern wldcore_projection_state_t g_wldcore_map_projection_state;
extern u8 g_wldcore_map_dot_pulse_direction;
extern u8 g_wldcore_map_dot_pulse_phase;
extern s32 g_wldcore_map_window_index;
void wldcore_map_init_dots(void);
void wldcore_map_init_location_marker_and_camera(void);
void wldcore_map_project_and_cull_dots(void);
void wldcore_map_pulse_dot_colors(void);
s32 wldcore_map_find_dot_at_point(point32_t point);
s32 wldcore_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y);

/* routes */
/* World-map routes: g_wldcore_map_route_tables and the flags from
 * EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST. */
enum { WLDCORE_ROUTE_COUNT = 48 };

enum { WLDCORE_ROUTE_PATH_CAPACITY = 16 };

/* A path of route legs, as the route search finds it. */
typedef struct wldcore_route_path {
    s32 routes[WLDCORE_ROUTE_PATH_CAPACITY];
} wldcore_route_path_t;
typedef char wldcore_route_path_size_must_be_64[sizeof(wldcore_route_path_t) == 64 ? 1 : -1];

/* A path entry packs the route id in its low byte and the direction in bit 8. */
enum {
    WLDCORE_ROUTE_PATH_ID_MASK = 0xff,
    WLDCORE_ROUTE_PATH_REVERSED = 0x100,
};

/* A travel point: packed x/y, a 32-bit heading, and segment distance. */
typedef struct wldcore_route_point {
    u32 position;
    u32 heading;
    s32 distance;
} wldcore_route_point_t;

/* A route header followed by count travel points. */
typedef struct wldcore_route_record {
    u8 count;
    u8 start_location;
    u8 end_location;
    u8 cost;
    wldcore_route_point_t points[0]; /* 0x04; variable length */
} wldcore_route_record_t;
typedef char wldcore_route_record_size_must_be_4[sizeof(wldcore_route_record_t) == 4 ? 1 : -1];

/* wldcore_slideshow_t.flags. */
typedef enum wldcore_slideshow_flag {
    WLDCORE_SLIDESHOW_RUNNING = 0x01,  /* the page timer runs */
    WLDCORE_SLIDESHOW_REVERSED = 0x02, /* the route is shown end to start */
} wldcore_slideshow_flag_e;

/* Provisional: a slideshow page list. The path animation (g_wldcore_map_path_animation) uses it for a route: the
 * route id is the script variable (offset by 0x22c, the route-visible variables), each page is one two-point segment
 * of the route polyline that follows the page list, shown for its length / 32 frames, and page i runs from point 2i
 * to point 2i + 2. */
typedef struct wldcore_slideshow {
    u16 flags;           /* 0x00; wldcore_slideshow_flag_e */
    s16 script_variable; /* 0x02; script variable base, offset by 0x22c */
    s16 page_count;      /* 0x04 */
    s16 page_index;      /* 0x06 */
    s16 page_frames[16]; /* 0x08; frames each page stays up */
    s16 frame_timer;     /* 0x28 */
    s16 point_count;     /* 0x2a */
    s16 coords[1];       /* 0x2c; packed (x, y) pairs */
} wldcore_slideshow_t;

/* The route records by route id: a pointer cell to the WLDCORE_ROUTE_COUNT record pointers. */
extern wldcore_route_record_t** g_wldcore_map_route_tables;
/* The route polylines by route id, as g_wldcore_map_route_tables: each is a packed s16 count followed by that many
 * (x, y) pairs. */
extern u16** g_wldcore_map_route_polylines;
extern s32 g_wldcore_route_search_depth;
extern s32 g_wldcore_route_search_cost;
extern wldcore_route_path_t g_wldcore_route_search_path;
extern wldcore_slideshow_t g_wldcore_map_path_animation;
void wldcore_map_draw_visible_routes(GsOT* ot);
void wldcore_map_prepare_path_animation(s32 from, s32 to);
void wldcore_map_draw_path_animation(GsOT* ot);

/* locations and route travel */
/* Provisional: four-byte per-location records reached through g_wldcore_location_records. */
typedef struct wldcore_state_record {
    u8 _unused_00[2];
    u8 picture; /* 0x02; proposition picture + 1, 0 for none */
    u8 is_town; /* 0x03; 1 offers the town entries WLDCORE_TEXT_ID_BAR to _SOLDIER_OFFICE in the location menu */
} wldcore_state_record_t;

/* wldcore_location_entry_state_t.flags. */
typedef enum wldcore_route_travel_flag {
    WLDCORE_ROUTE_TRAVEL_ACTIVE = 0x01,   /* a path was found and is being traveled */
    WLDCORE_ROUTE_TRAVEL_REVERSED = 0x02, /* the current leg runs its route end to start */
} wldcore_route_travel_flag_e;

/* Provisional: the route travel state (g_wldcore_location_entry_state), filled by the route search
 * wldcore_map_find_best_route_path, staged per route leg by wldcore_location_begin_route_segment and stepped by
 * wldcore_map_step_route_travel. The vertices are SVECTORs: their 2-byte alignment is what makes
 * `previous_position = position` take GCC's unaligned block-move path (lwl/lwr, swl/swr) that the target emits. */
typedef struct wldcore_location_entry_state {
    s32 flags;                       /* 0x00; wldcore_route_travel_flag_e */
    s32 route_id;                    /* 0x04; route & 0xff, index into g_wldcore_map_route_tables */
    s32 segment_index;               /* 0x08; current segment of the leg, the step for begin_route_segment */
    s32 segment_count;               /* 0x0c; wldcore_location_begin_route_segment result (record count - 1) */
    s32 segment_frame;               /* 0x10; frames into the segment, interpolation numerator */
    s32 segment_frames;              /* 0x14; point distance * 2, interpolation denominator */
    wldcore_route_path_t route_path; /* 0x18; best path found */
    s32 route_index;                 /* 0x58; route_path.routes index of the current leg */
    s32 best_path_length;            /* 0x5c */
    s32 best_cost;                   /* 0x60; reset to 99999 */
    s32 best_length_bound;           /* 0x64; reset to 99999 */
    s32 leg_start_location;          /* 0x68; route record start (end when reversed) */
    s32 leg_end_location;            /* 0x6c; becomes the current location and script variable 0x31 on arrival */
    s32 heading;                     /* 0x70; route point heading (+0x800 when reversed), copied to the marker angle */
    s32 menu_level;                  /* 0x74; parent menu depth */
    SVECTOR previous_position;       /* 0x78 */
    SVECTOR position;                /* 0x80 */
    SVECTOR target_position;         /* 0x88 */
    s32 arrival_location;            /* 0x90; arrival location id + 1 */
} wldcore_location_entry_state_t;
typedef char wldcore_location_entry_state_size_must_be_148[sizeof(wldcore_location_entry_state_t) == 148 ? 1 : -1];

extern wldcore_state_record_t* g_wldcore_location_records;
extern wldcore_location_entry_state_t g_wldcore_location_entry_state;
extern s32 g_wldcore_location_marker_shade;
extern u32* g_wldcore_location_frame_tim;
/* Per location choice, its offset from EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST: a set variable hides the
 * choice from the location menu (wldcore_map_build_location_menu_entries). */
extern u8 g_wldcore_choice_hidden_variable_offsets[];
/* Indexed so the store in wldcore_location_process_entry stays behind the
 * window-flag store (MEM_IN_STRUCT_P). */
extern s32 g_wldcore_next_map_id[];
s32 wldcore_location_process_entry(s32 from_location, s32 to_location);
s32 wldcore_map_build_location_menu_entries(s32 proposition_id, s32* out);
void wldcore_map_step_route_travel(void);

/* random encounters */
/* One random-encounter tier: the story-progress threshold that selects it,
 * the chance an encounter happens (a roll of 0..99 at or below it), and the
 * high-bit-first mask of which of the entry's eight ENTDs may be drawn. */
typedef struct wldcore_encounter_tier {
    u8 progress_threshold; /* 0x00 */
    u8 encounter_chance;   /* 0x01 */
    u8 encounter_mask;     /* 0x02 */
} wldcore_encounter_tier_t;

/* One road into a world-map location. Three per location, 19 locations
 * (ids 24..42), reached through the pointer cell g_wldcore_random_battle_data. */
typedef struct wldcore_encounter_entry {
    u8 route;                          /* 0x00 */
    u8 deployment_squad_id;            /* 0x01: deployment zone id = this + 0x200 */
    wldcore_encounter_tier_t tiers[4]; /* 0x02 */
    u8 entd_ids[8];                    /* 0x0e */
    u8 battle_map_id;                  /* 0x16 */
    u8 script_variable_id;             /* 0x17 */
} wldcore_encounter_entry_t;

typedef struct wldcore_deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 entd_ids[8];
} wldcore_deep_dungeon_battle_t;

extern wldcore_encounter_entry_t (*g_wldcore_random_battle_data)[3];
extern s32 g_wldcore_random_battle_entd_id;
extern s32 g_wldcore_random_battle_map_id;
extern s32 g_wldcore_random_battle_squad_id;
extern wldcore_deep_dungeon_battle_t* g_wldcore_deep_dungeon_battles;

#endif

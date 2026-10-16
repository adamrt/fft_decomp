#ifndef WLDCORE_MAP_H
#define WLDCORE_MAP_H

/* WLDCORE's world map: tiles, dots, routes, locations, the camera and random encounters. */

#include "fft/gfx.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* random encounter */
extern s32 g_wldcore_random_battle_entd_id;
extern s32 g_wldcore_random_battle_map_id;
extern s32 g_wldcore_random_battle_squad_id;
extern wldcore_encounter_entry_t (*g_wldcore_random_battle_data)[3];

/* route */
extern s32 g_wldcore_route_search_depth;
extern s32 g_wldcore_route_search_cost;
extern wldcore_route_path_t g_wldcore_route_search_path;

/* location */
extern wldcore_location_entry_state_t g_wldcore_location_entry_state;
extern wldcore_state_record_t* g_wldcore_location_records;
extern s32 g_wldcore_location_marker_shade;
extern u32* g_wldcore_location_frame_tim;
s32 wldcore_location_process_entry(s32 from_location, s32 to_location);

/* camera */
extern wldcore_map_dot_source_t* g_wldcore_map_dot_sources;

/* map */
extern s32 g_wldcore_map_dot_count;
extern wldcore_map_dot_t g_wldcore_map_dots[];
extern wldcore_projection_state_t g_wldcore_map_projection_state;
extern wldcore_projection_motion_t g_wldcore_map_projection_motion;
extern wldcore_projected_entry_t g_wldcore_map_projected_tiles[];
/* Pointer to the 48 route polylines (array at 0x80095fb0), one per route;
 * each is a packed s16 count followed by that many (x, y) pairs (0x80096070
 * holds count 12 and the next record starts at 0x800960a2). A sibling of
 * g_wldcore_map_route_tables, indexed by the same route id. */
extern u16** g_wldcore_map_route_polylines;
/* Pointer cell at 0x80095000; the 48 route-record pointers begin at 0x80095008. */
extern wldcore_route_record_t** g_wldcore_map_route_tables;
/* The horizontal and vertical map scroll axes. */
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_x;
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_y;
extern u8 g_wldcore_map_dot_pulse_direction;
extern u8 g_wldcore_map_dot_pulse_phase;
extern wldcore_slideshow_t g_wldcore_map_path_animation;
extern s32 g_wldcore_map_window_index;
extern wldcore_map_projection_view_t g_wldcore_map_projection_view;
extern VECTOR g_wldcore_map_projection_origin;
extern SVECTOR g_wldcore_map_projection_rotation;
extern VECTOR g_wldcore_map_projection_scale;
extern s32* g_wldcore_map_tile_descriptors;
/* Provisional: the zoom animation record at 0x800d3c8c; flags bit 0 runs
 * the animation, bit 1 selects the zoomed-out direction. */
extern wldcore_zoom_motion_t g_wldcore_map_zoom_motion;
void wldcore_map_init_tile_atlas(void);
void wldcore_map_color_and_draw_dots(void);
s32 wldcore_map_build_location_menu_entries(s32 proposition_id, s32* out);
void wldcore_map_draw_visible_routes(GsOT* ot);
void wldcore_map_draw_path_animation(GsOT* ot);
void wldcore_map_step_route_travel(void);
s32 wldcore_map_step_projection_motion(void);
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position);
void wldcore_map_apply_clamped_scroll(point32_t* amount, VECTOR* position);
void wldcore_map_apply_clamped_vertical_scroll(point32_t* amount, VECTOR* position);
s32 wldcore_map_find_dot_at_point(point32_t point);
void wldcore_map_init_location_marker_and_camera(void);
s32 wldcore_map_is_point_outside_projection_bounds(const point32_t* point);
void wldcore_map_project_and_cull_dots(void);
void wldcore_map_project_and_cull_tiles(void);
void wldcore_map_pulse_dot_colors(void);
s32 wldcore_map_start_projection_motion_if_outside_bounds(const point32_t* point, const SVECTOR* coordinates);
void wldcore_map_update_camera_direction_and_projection(void);
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction);
void wldcore_map_prepare_path_animation(s32 from, s32 to);
s32 wldcore_map_get_dot_snap_step(point32_t point, s32* step_x, s32* step_y);

/* menu */
void wldcore_map_step_projection_motion_then_pop_level(wldcore_menu_stack_record_t* entry);

#endif

#ifndef BATTLE_GFX_H
#define BATTLE_GFX_H

/* Battle rendering: the render buffers, unit sprites and shadows, palettes, result and status
 * displays, cursors and full-screen color effects. The sprite files they draw from are in
 * battle_gfx_resource.h. */

#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* render buffers */
/* Primitive pool sizes of a render buffer. The map mesh's position records (battle_map_mesh.h)
 * parallel the four map polygon pools; the weather sprites are the flat-textured triangles. */
enum {
    RENDER_TEXTURED_TRIANGLE_COUNT = 360,
    RENDER_TEXTURED_QUAD_COUNT = 710,
    RENDER_UNTEXTURED_TRIANGLE_COUNT = 64,
    RENDER_UNTEXTURED_QUAD_COUNT = 256,
    RENDER_WEATHER_SPRITE_COUNT = 64,
};

/* One of the two 60968-byte battle render buffers. The primitive pools are
 * pre-initialized once; gfx_swap_and_clear_battle_otag reads the ordering table
 * pointer at +0xe5b4. */
typedef struct render_buffer {
    POLY_GT3 gt3[RENDER_TEXTURED_TRIANGLE_COUNT]; /* 0x00000 */
    POLY_GT4 gt4[RENDER_TEXTURED_QUAD_COUNT];     /* 0x03840 */
    POLY_F3 f3[RENDER_UNTEXTURED_TRIANGLE_COUNT]; /* 0x0c878 */
    POLY_F4 f4[RENDER_UNTEXTURED_QUAD_COUNT];     /* 0x0cd78 */
    POLY_G4 overlay;                              /* 0x0e578 */
    DR_MODE overlay_modes[2];                     /* 0x0e59c */
    u32* otag;                                    /* 0x0e5b4 */
    u8 _unused_e5b8[0x70];                        /* 0x0e5b8 */
    POLY_FT3 ft3[RENDER_WEATHER_SPRITE_COUNT];    /* 0x0e628 */
} render_buffer_t;
typedef char render_buffer_size_must_be_60968[(sizeof(render_buffer_t) == 60968) ? 1 : -1];

/* The buffer being built this frame; gfx_swap_and_clear_battle_otag alternates
 * it between the two g_render_buffers entries. */
extern render_buffer_t* g_render_current_buffer;
extern render_buffer_t g_render_buffers[2];

void render_init_buffers(void);

/* sprite displays */
/* g_sprite_prim_pool is a ring of 256 POLY_FT4; g_sprite_prim_index is the index of
 * the last one taken, and g_sprite_prim_frame_start_index its value when the frame began. */
extern s16 g_sprite_prim_index;
extern s16 g_sprite_prim_frame_start_index;
extern POLY_FT4* g_sprite_prim_pool;

s16 sprite_take_prim(void);
void sprite_display_construct_polygon_data(
    sprite_display_t* display, s32 end, s32 start, SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot);
sprite_display_t* sprite_display_init_item(s32 runtime_id);
sprite_display_t* sprite_display_init_unit(s32 runtime_id);
sprite_display_t* sprite_display_init_weapon(s32 unit_index, s32 display_type);
void sprite_display_release_unit_stub(sprite_display_t* unused_display);
void sprite_display_set_part_flip(sprite_display_t* display, s32 part_index, u32 flags);
void sprite_display_set_part_semi_transparent(sprite_display_t* display, s32 part_index, s32 enabled);
void sprite_display_store_data(
    sprite_display_t* display, s32 index, s16 x_shift, s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags);

/* unit sprites */
/* The 24 orders in which sprite_draw_unit_layers queues a unit's
 * four layers: 0 is the unit's own sprite, n its sprite block n - 1. */
extern s32 g_animation_layer_priorities[][4];
/* The display rotations that the upper five bits of a frame's first byte select, ONE to the turn. */
extern u16 g_sprite_y_rotations[];

void sprite_sheet_apply_status_change(unit_t* unit, s32 tile_effect_level);
void sprite_afterimage_draw(unit_t* unit);
sprite_frame_t* sprite_get_unit_frame_and_vram_ids(u32 runtime_id, sprite_unit_query_t* out);
void unit_hide_all_jumping(void);
void sprite_draw_item_display(struct unit* unit);
void sprite_afterimage_init(struct unit* unit);
void sprite_frame_load_unit(unit_t* unit, animation_state_t* state, u16 frame, u16 sequence_id);
s32 unit_hide_by_runtime_id(u32 runtime_id);
void sprite_afterimage_shift_and_fill(struct unit* unit);
void sprite_display_store_item(struct unit* unit, u32 item_id);
void sprite_update_transparency_flag(struct unit* unit);

/* shadows */
void sprite_shadow_build_quad(unit_t* unit);
void sprite_shadow_draw(unit_t* unit, u32* ot);
s32 sprite_shadow_hide_by_runtime_id(u32 runtime_id);
s32 sprite_shadow_show_by_runtime_id(u32 runtime_id);

/* projection */

s32 unit_height_calculate_from_battle_tile(struct unit* unit);
s32 unit_height_calculate_from_tile(struct unit* unit);
s32 unit_height_calculate_from_move_destination(struct unit* unit);
/* The definition returns an s16, but an s16 prototype makes the callers re-extend the result;
 * keep s32. */
s32 unit_height_calculate_from_world_coords(struct unit* unit);
s32 unit_height_calculate_with_caller_coords(struct unit* unit, const SVECTOR* coords);
void sprite_project_all_units(void);

/* palettes */
/* First 21 OTHER.SPR palettes copied together; the other three groups also
 * retain their independently loaded symbol aliases. */
typedef struct sprite_other_palettes {
    palette_t chicken[5];  /* 0x000 */
    palette_t frog[5];     /* 0x0a0 */
    palette_t crystal[6];  /* 0x140 */
    palette_t treasure[5]; /* 0x200 */
} sprite_other_palettes_t;
typedef char sprite_other_palettes_size_must_be_672[sizeof(sprite_other_palettes_t) == 672 ? 1 : -1];

extern palette_t g_sprite_crystal_palettes[6];
extern palette_t g_cursor_tile_glow_cycled_palettes[2];
extern clut_t g_sprite_trail_clut;
extern palette_t g_sprite_frog_palettes[5];
extern clut_t g_sprite_weapon_clut; /* the first 0x200 bytes of WEP.SPR */
extern sprite_other_palettes_t g_sprite_other_palettes;
extern palette_t g_sprite_shadow_palette;
extern palette_t g_sprite_treasure_palettes[5];

void sprite_palette_reset_by_runtime_id(u32 runtime_id);
void sprite_palette_check_tile_status_mod(struct unit* unit, s32 tile_effect_level, s32 final_value);
void unit_fade_out_start(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue);
void unit_fade_out_start_red(s32 runtime_id);
void unit_fade_out_start_for_event(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue);
void sprite_palette_tint_by_team(struct unit* unit, s32 amount);
void battle_map_palette_modify_from_other(s32 mode, const rgba5551_t* source, s32 clut_row, s32 clut_index,
    s32 all_colors, s16 red, s16 green, s16 blue, s32 final_value);
void sprite_evtchr_set_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index);
void sprite_palette_set_thrown_item(s32 item_id, unit_t* unit);
void sprite_palette_set_thrown_item_by_runtime_id(u32 item_id, u32 runtime_id);
void sprite_palette_apply_modulation_to_units(s32 mode, s32 amount, s32 runtime_id, s16 red, s16 green, s16 blue);
void sprite_palette_tint_all_by_team(void);
void sprite_palette_tint_unit(unit_t* unit, s32 level, s32 final_value);
void cursor_cycle_tile_glow_palettes(void);
void sprite_palette_restore_all(void);

/* result and status displays */
/* unit_t.status_bubble_id: the bubbles the code names. The others follow the statuses of
 * g_status_bubble_status_masks. */
typedef enum status_bubble {
    STATUS_BUBBLE_DEAD = 0, /* the death counter, or the RERAISE image under Reraise */
    STATUS_BUBBLE_CHARGING = 9,
    STATUS_BUBBLE_RERAISE = 10,
    STATUS_BUBBLE_DEATH_SENTENCE = 20, /* its counter */
    STATUS_BUBBLE_SOURCE_UNIT = 21,    /* the source unit's marker, not a status */
    STATUS_BUBBLE_COUNT = 22,
} status_bubble_e;

extern MATRIX g_sprite_display_matrix;
extern s32 g_status_bubble_delay;
extern palette_t g_status_bubble_palette;
/* Status flag shown by each status bubble before SOURCE_UNIT. DEAD and DEATH_SENTENCE hold 0 and are
 * tested separately by status_bubble_determine_parameters; bubbles 8 and 11 hold 0 too,
 * so they never show. */
extern s32 g_status_bubble_status_masks[STATUS_BUBBLE_SOURCE_UNIT];
extern s16 g_result_digit_0_critical_zoom_curve[21];
extern s16 g_result_digit_0_zoom_curve[21];
extern s16 g_result_digit_1_critical_zoom_curve[21];
extern s16 g_result_digit_1_zoom_curve[21];
extern s16 g_result_digit_2_critical_zoom_curve[21];
extern s16 g_result_digit_2_zoom_curve[21];

void result_queue_charge_failure_message(unit_t* unit, s32 mode);
void result_animate_text(unit_t* unit, const SVECTOR* screen_coords);
s32 result_build_next_display(unit_t* unit);
s32 result_build_next_special_display(unit_t* unit);
void status_bubble_draw(unit_t* unit, const DVECTOR* unused_screen_coords);
void result_init_earned_exp_jp_display(struct unit* unit);
sprite_display_t* sprite_display_init_numeric(s32 unit_index, s32 display_section);
void unit_update_float_height_and_status_bubble(unit_t* unit);
sprite_display_t* sprite_display_init_status_bubble(s32 unit_index);
void result_prepare_display(unit_t* unit);
s32 result_prepare_display_by_runtime_id(s32 runtime_id);
void status_bubble_update_active(struct unit* unit);
void sprite_update_status_bubbles_and_graphics(void);

/* cursors */
extern palette_t g_cursor_map_selection_palette;
/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_cursor_selection_bob_timer;
extern s32 g_cursor_selection_bob_frame;
/* Advanced by g_system_vsync_interval once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
extern u32 g_unit_facing_wait_arrow_blink_timer;
extern palette_t g_unit_facing_wait_arrow_palette;

void cursor_build_tile_glow(s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot);
void cursor_draw_map_selection(SVECTOR* base, u32* ot);
void unit_facing_draw_wait_arrows(SVECTOR* vector, u32* ot);
void unit_facing_draw_wait_direction(void);

/* screen effects */
/* Commands of battle_map_light_control_back_color, which stores RGB and controls the GTE
 * background color. */
typedef enum battle_map_light_back_color_command {
    BATTLE_MAP_LIGHT_BACK_COLOR_SET = 9,
    BATTLE_MAP_LIGHT_BACK_COLOR_GET = 10,
    BATTLE_MAP_LIGHT_BACK_COLOR_DISABLE = 11,
    BATTLE_MAP_LIGHT_BACK_COLOR_RESTORE = 12
} battle_map_light_back_color_command_e;

extern rgb24_t g_fade_tint;
extern rgb24_t g_fade_tint_backup;
extern u16 g_fade_tint_fade_frames;
extern u16 g_fade_tint_step[3];
extern rgb24_t g_fade_tint_target;
extern u16 g_fade_tint_value[3];
extern fade_overlay_t g_fade_overlays[]; /* one fade overlay per packet buffer */
/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_fade_tint_draw_modes[2];
extern POLY_F4 g_fade_tint_polygons[2];
extern RECT g_fade_tint_tex_window;
extern DR_MODE g_fade_draw_modes[];
extern s16 g_fade_intensity;
extern POLY_F4 g_fade_polygons[2];

void fade_tint_draw_overlay(void);
void fade_tint_start_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void fade_tint_update_fade(void);

#endif

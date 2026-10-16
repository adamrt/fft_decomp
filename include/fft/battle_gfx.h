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
    BATTLE_RENDER_TEXTURED_TRIANGLE_COUNT = 360,
    BATTLE_RENDER_TEXTURED_QUAD_COUNT = 710,
    BATTLE_RENDER_UNTEXTURED_TRIANGLE_COUNT = 64,
    BATTLE_RENDER_UNTEXTURED_QUAD_COUNT = 256,
    BATTLE_RENDER_WEATHER_SPRITE_COUNT = 64,
};

/* One of the two 60968-byte battle render buffers. The primitive pools are
 * pre-initialized once; main_gfx_swap_and_clear_otag reads the ordering table
 * pointer at +0xe5b4. */
typedef struct battle_render_buffer {
    POLY_GT3 gt3[BATTLE_RENDER_TEXTURED_TRIANGLE_COUNT]; /* 0x00000 */
    POLY_GT4 gt4[BATTLE_RENDER_TEXTURED_QUAD_COUNT];     /* 0x03840 */
    POLY_F3 f3[BATTLE_RENDER_UNTEXTURED_TRIANGLE_COUNT]; /* 0x0c878 */
    POLY_F4 f4[BATTLE_RENDER_UNTEXTURED_QUAD_COUNT];     /* 0x0cd78 */
    POLY_G4 overlay;                                     /* 0x0e578 */
    DR_MODE overlay_modes[2];                            /* 0x0e59c */
    u32* otag;                                           /* 0x0e5b4 */
    u8 _unused_e5b8[0x70];                               /* 0x0e5b8 */
    POLY_FT3 ft3[BATTLE_RENDER_WEATHER_SPRITE_COUNT];    /* 0x0e628 */
} battle_render_buffer_t;
typedef char battle_render_buffer_size_must_be_60968[(sizeof(battle_render_buffer_t) == 60968) ? 1 : -1];

/* The buffer being built this frame; main_gfx_swap_and_clear_otag alternates
 * it between the two g_battle_render_buffers entries. */
extern battle_render_buffer_t* g_battle_gfx_current_render_buffer;
extern battle_render_buffer_t g_battle_render_buffers[2];

void battle_gfx_init_render_buffers(void);

/* sprite displays */
/* g_battle_gfx_sprite_prim_pool is a ring of 256 POLY_FT4; g_battle_gfx_sprite_prim_index is the index of
 * the last one taken, and g_battle_gfx_frame_start_sprite_prim_index its value when the frame began. */
extern s16 g_battle_gfx_sprite_prim_index;
extern s16 g_battle_gfx_frame_start_sprite_prim_index;
extern POLY_FT4* g_battle_gfx_sprite_prim_pool;

s16 battle_gfx_take_sprite_prim(void);
void battle_gfx_construct_polygon_data_for_units(battle_gfx_sprite_display_data_t* display, s32 end, s32 start,
    SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot);
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 runtime_id);
battle_gfx_sprite_display_data_t* battle_gfx_init_unit_sprite_display(s32 runtime_id);
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type);
void battle_gfx_release_unit_sprite_display_stub(battle_gfx_sprite_display_data_t* unused_display);
void battle_gfx_set_sprite_part_flip(battle_gfx_sprite_display_data_t* display, s32 part_index, u32 flags);
void battle_gfx_set_sprite_part_semi_transparent(
    battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled);
void battle_gfx_store_sprite_display_data(battle_gfx_sprite_display_data_t* display, s32 index, s16 x_shift,
    s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags);

/* unit sprites */
/* The 24 orders in which battle_gfx_draw_unit_sprite_layers queues a unit's
 * four layers: 0 is the unit's own sprite, n its sprite block n - 1. */
extern s32 g_battle_gfx_animation_layer_priorities[][4];
/* The display rotations that the upper five bits of a frame's first byte select, ONE to the turn. */
extern u16 g_battle_gfx_sprite_y_rotations[];

void battle_gfx_apply_status_spritesheet_change(battle_unit_t* unit, s32 tile_effect_level);
void battle_gfx_draw_unit_sprite_position_copies(battle_unit_t* unit);
battle_gfx_source_frame_t* battle_gfx_get_unit_sprite_frame_and_vram_ids(
    u32 runtime_id, battle_unit_sprite_query_t* out);
void battle_gfx_hide_jumping_units(void);
void battle_gfx_init_item_get_rendering(struct battle_unit* unit);
void battle_gfx_init_position_vector_copies(struct battle_unit* unit);
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id);
s32 battle_gfx_reset_unit_graphic_trigger(u32 runtime_id);
void battle_gfx_shift_and_fill_position_copies(struct battle_unit* unit);
void battle_gfx_store_item_display_data(struct battle_unit* unit, u32 item_id);
void battle_gfx_update_sprite_transparency_flag(struct battle_unit* unit);

/* shadows */
void battle_gfx_build_unit_shadow_quad(battle_unit_t* unit);
void battle_gfx_draw_unit_shadow(battle_unit_t* unit, u32* ot);
s32 battle_gfx_hide_unit_shadow_by_runtime_id(u32 runtime_id);
s32 battle_gfx_show_unit_shadow_by_runtime_id(u32 runtime_id);

/* projection */

s32 battle_gfx_calculate_world_height_from_battle_tile(struct battle_unit* unit);
s32 battle_gfx_calculate_world_height_from_unit_tile(struct battle_unit* unit);
s32 battle_gfx_calculate_world_height_from_move_destination(struct battle_unit* unit);
/* The definition returns an s16, but an s16 prototype makes the callers re-extend the result;
 * keep s32. */
s32 battle_gfx_calculate_world_height_from_world_coords(struct battle_unit* unit);
s32 battle_gfx_calculate_world_height_with_caller_coords(struct battle_unit* unit, const SVECTOR* coords);
void battle_gfx_project_all_units(void);

/* palettes */
/* First 21 OTHER.SPR palettes copied together; the other three groups also
 * retain their independently loaded symbol aliases. */
typedef struct battle_gfx_other_palettes {
    palette_t chicken[5];  /* 0x000 */
    palette_t frog[5];     /* 0x0a0 */
    palette_t crystal[6];  /* 0x140 */
    palette_t treasure[5]; /* 0x200 */
} battle_gfx_other_palettes_t;
typedef char battle_gfx_other_palettes_size_must_be_672[sizeof(battle_gfx_other_palettes_t) == 672 ? 1 : -1];

extern palette_t g_battle_gfx_crystal_palettes[6];
extern palette_t g_battle_gfx_cycled_deployed_palettes[2];
extern clut_t g_battle_gfx_trail_clut;
extern palette_t g_battle_gfx_frog_palettes[5];
extern clut_t g_battle_gfx_weapon_clut; /* the first 0x200 bytes of WEP.SPR */
extern battle_gfx_other_palettes_t g_battle_gfx_other_palettes;
extern palette_t g_battle_gfx_shadow_palette;
extern palette_t g_battle_gfx_treasure_palettes[5];

void battle_gfx_reset_unit_palette_by_runtime_id(u32 runtime_id);
void battle_gfx_check_tile_status_palette_mod(struct battle_unit* unit, s32 tile_effect_level, s32 final_value);
void battle_gfx_start_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_start_unit_fade_out_red(s32 runtime_id);
void battle_gfx_start_event_unit_fade_out(s32 runtime_id, s32 mode, s16 red, s16 green, s16 blue);
void battle_gfx_modify_palette_based_on_team(struct battle_unit* unit, s32 amount);
void battle_gfx_modify_vram_palette(s32 mode, const rgba5551_t* source, s32 clut_row, s32 clut_index, s32 all_colors,
    s16 red, s16 green, s16 blue, s32 final_value);
void battle_gfx_set_evtchr_palette_by_runtime_id(s32 runtime_id, s32 slot_index, s32 clut_index);
void battle_gfx_set_thrown_item_graphic_palette(s32 item_id, battle_unit_t* unit);
void battle_gfx_set_thrown_item_palette_by_runtime_id(u32 item_id, u32 runtime_id);
void battle_gfx_apply_unit_palette_modulation_to_units(
    s32 mode, s32 amount, s32 runtime_id, s16 red, s16 green, s16 blue);
void battle_gfx_tint_all_units_by_team(void);
void battle_gfx_tint_unit_palette(battle_unit_t* unit, s32 level, s32 final_value);
void battle_gfx_update_unit_palette_animation(void);
void battle_gfx_restore_unit_palettes(void);

/* result and status displays */
/* battle_unit_t.status_bubble_id: the bubbles the code names. The others follow the statuses of
 * g_battle_gfx_status_bubble_status_masks. */
typedef enum battle_status_bubble {
    BATTLE_STATUS_BUBBLE_DEAD = 0, /* the death counter, or the RERAISE image under Reraise */
    BATTLE_STATUS_BUBBLE_CHARGING = 9,
    BATTLE_STATUS_BUBBLE_RERAISE = 10,
    BATTLE_STATUS_BUBBLE_DEATH_SENTENCE = 20, /* its counter */
    BATTLE_STATUS_BUBBLE_SOURCE_UNIT = 21,    /* the source unit's marker, not a status */
    BATTLE_STATUS_BUBBLE_COUNT = 22,
} battle_status_bubble_e;

extern MATRIX g_battle_gfx_numeric_display_matrix;
extern s32 g_battle_gfx_status_bubble_delay;
extern palette_t g_battle_gfx_status_bubble_palette;
/* Status flag shown by each status bubble before SOURCE_UNIT. DEAD and DEATH_SENTENCE hold 0 and are
 * tested separately by battle_gfx_determine_status_bubble_parameters; bubbles 8 and 11 hold 0 too,
 * so they never show. */
extern s32 g_battle_gfx_status_bubble_status_masks[BATTLE_STATUS_BUBBLE_SOURCE_UNIT];
extern s16 g_battle_numeric_digit_0_critical_zoom_curve[21];
extern s16 g_battle_numeric_digit_0_zoom_curve[21];
extern s16 g_battle_numeric_digit_1_critical_zoom_curve[21];
extern s16 g_battle_numeric_digit_1_zoom_curve[21];
extern s16 g_battle_numeric_digit_2_critical_zoom_curve[21];
extern s16 g_battle_numeric_digit_2_zoom_curve[21];

void battle_gfx_queue_charge_failure_message(battle_unit_t* unit, s32 mode);
void battle_gfx_animate_post_action_text(battle_unit_t* unit, const SVECTOR* screen_coords);
s32 battle_gfx_build_next_action_result_display(battle_unit_t* unit);
s32 battle_gfx_build_next_special_action_result_display(battle_unit_t* unit);
void battle_gfx_draw_status_bubble(battle_unit_t* unit, const DVECTOR* unused_screen_coords);
void battle_gfx_init_earned_exp_jp_display(struct battle_unit* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(s32 unit_index, s32 display_section);
void battle_gfx_update_float_height_and_status_bubble(battle_unit_t* unit);
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index);
void battle_gfx_prepare_post_action_display(battle_unit_t* unit);
s32 battle_gfx_prepare_post_action_display_by_runtime_id(s32 runtime_id);
void battle_gfx_update_status_bubble_graphic_trigger(struct battle_unit* unit);
void battle_gfx_update_status_bubbles_and_graphics(void);

/* cursors */
extern palette_t g_battle_gfx_map_selection_cursor_palette;
/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_battle_gfx_selection_cursor_bob_timer;
extern s32 g_battle_gfx_selection_cursor_bob_frame;
/* Advanced by g_main_system_vsync_interval once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
extern u32 g_battle_gfx_wait_arrow_blink_timer;
extern palette_t g_battle_gfx_wait_direction_arrow_palette;

void battle_gfx_build_cursor_tile_glow(
    s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot);
void battle_gfx_draw_map_selection_cursor(SVECTOR* base, u32* ot);
void battle_gfx_draw_wait_direction_arrows(SVECTOR* vector, u32* ot);
void battle_gfx_draw_wait_direction_unit(void);

/* screen effects */
/* Commands of battle_map_control_gte_background_color, which stores RGB and controls the GTE
 * background color. */
typedef enum battle_background_color_command {
    BATTLE_BACKGROUND_COLOR_SET = 9,
    BATTLE_BACKGROUND_COLOR_GET = 10,
    BATTLE_BACKGROUND_COLOR_DISABLE = 11,
    BATTLE_BACKGROUND_COLOR_RESTORE = 12
} battle_background_color_command_e;

extern rgb24_t g_battle_gfx_screen_color_modulation;
extern rgb24_t g_battle_gfx_screen_color_modulation_backup;
extern u16 g_battle_gfx_screen_color_modulation_fade_frames;
extern u16 g_battle_gfx_screen_color_modulation_step[3];
extern rgb24_t g_battle_gfx_screen_color_modulation_target;
extern u16 g_battle_gfx_screen_color_modulation_value[3];
extern fade_overlay_t g_battle_gfx_screen_fade_overlays[]; /* one fade overlay per packet buffer */
/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_battle_gfx_screen_modulation_draw_modes[2];
extern POLY_F4 g_battle_gfx_screen_modulation_polygons[2];
extern RECT g_battle_gfx_screen_modulation_tex_window;
extern DR_MODE g_battle_screen_fade_draw_modes[];
extern s16 g_battle_screen_fade_intensity;
extern POLY_F4 g_battle_screen_fade_polygons[2];

void battle_gfx_draw_screen_color_modulation_overlay(void);
void battle_gfx_start_screen_color_modulation_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void battle_gfx_update_screen_color_modulation_fade(void);

#endif

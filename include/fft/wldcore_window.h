#ifndef WLDCORE_WINDOW_H
#define WLDCORE_WINDOW_H

/* WLDCORE's window records, panels and screen fades. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Provisional: 16-byte rows of the window-layout table at 0x8009e93c.
 * wldcore_menu_handle_layout_window_input reads the first ten bytes: a pointer
 * to the level's live counter, the base message id, and the four neighboring
 * layout ids reached by the directional pad. wldcore_window_apply_layout reads
 * the trailing 16-bit values: the record's x and y extents (plus 14 and 12) and the
 * kind that selects the redraw delay. */
typedef struct wldcore_window_layout_row {
    s16* counter;     /* 0x00 */
    u16 message_base; /* 0x04 */
    u8 up;            /* 0x06 */
    u8 down;          /* 0x07 */
    u8 right;         /* 0x08 */
    u8 left;          /* 0x09 */
    s16 x;            /* 0x0a */
    s16 y;            /* 0x0c */
    s16 kind;         /* 0x0e */
} wldcore_window_layout_row_t;
typedef char wldcore_window_layout_row_size_must_be_16[sizeof(wldcore_window_layout_row_t) == 16 ? 1 : -1];

enum { WLDCORE_WINDOW_RECORD_CAPACITY = 32 };

/* Text width is in pixels; the second value counts visible list rows. */
typedef struct wldcore_window_panel_layout {
    s32 text_width;
    s32 visible_rows;
} wldcore_window_panel_layout_t;

/* gfx */
extern GsOT_TAG g_wldcore_gfx_aux_ot_tags[2][4];
extern GsOT g_wldcore_gfx_aux_ordering_tables[2];
extern GsOT_TAG g_wldcore_gfx_ot_tags[2][16];
extern GsOT g_wldcore_gfx_ordering_tables[2];
extern PACKET g_wldcore_gfx_world_primitive_buffers[];
s32 wldcore_fade_step_screen(void);
void wldcore_gfx_toggle_captured_world_frame(s32 capture);
void wldcore_gfx_draw_projected_map_tiles(GsOT* ot);
void wldcore_gfx_draw_animated_display_object(world_animated_render_record_t* object, GsOT* ot);
void wldcore_gfx_draw_context_value_display(GsOT* ot);
void wldcore_gfx_draw_calendar_date(GsOT* ot);
void wldcore_fade_draw_screen_overlays(void);
void wldcore_fade_start_screen(s32 flags, s32 duration);
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch);
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out);
void wldcore_gfx_draw_world_frame(void);
void wldcore_gfx_set_display_rect(s32 buffer_index, RECT* rect);
void wldcore_gfx_load_background_set(s32 set);

/* text */
extern s32 g_wldcore_displayed_numeric_value;
extern s32 g_wldcore_displayed_numeric_value_secondary;
extern s32 g_wldcore_displayed_numeric_value_tertiary;

/* menu */
extern s32 g_wldcore_window_aux_render_object_count;
extern void* g_wldcore_window_aux_render_object_queue[];
/* Shared panel renderer state at 0x800bb31c, filled by
 * wldcore_window_init_panel_render_state and
 * wldcore_window_init_number_panel_render_thread and read by
 * world_screen_build_at_list. Its text binding is the label layout at 0x800bb394
 * (column mode 0 text, 2 hidden, 3 alternate number) or, for number panels,
 * g_wldcore_window_panel_number_state. */
extern menu_text_layout_t g_wldcore_window_panel_label_state;
extern menu_entry_t g_wldcore_window_panel_render_state;
extern s32 g_wldcore_window_record_count;
extern world_animated_render_record_t g_wldcore_window_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern wldcore_window_layout_row_t g_wldcore_window_layout_rows[];
extern s32 g_wldcore_window_render_object_count;
/* Display-object queues drawn by 0x8006ae20 each frame: every entry points at
 * the leading 32-bit flags of a window record, render record or map dot, and
 * wldcore_window_set_render_objects_visible toggles bit 0x08 through it. */
extern void* g_wldcore_window_render_object_queue[];
extern wldcore_window_render_record_t g_wldcore_window_render_records[WLDCORE_WINDOW_RECORD_CAPACITY];
extern s32 g_wldcore_window_render_record_count;
extern wldcore_window_xy_t g_wldcore_window_record_positions[];
extern u32 g_wldcore_window_image_buffer[];
extern struct world_menu_number_range g_wldcore_window_panel_number_state;
void wldcore_window_build_yes_no_panel(point32_t origin);
/* Menu resume callbacks consume the variant selected by the level tag. */
void wldcore_window_clear_layout_states(wldcore_menu_stack_record_t* level);
void wldcore_window_restore_proposition_detail_palette(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_sound_novel_level(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_pair_state(wldcore_menu_stack_record_t* level);
void wldcore_window_clear_slot_state(wldcore_menu_stack_record_t* level);
void wldcore_window_restore_location_list_palette(wldcore_menu_stack_record_t* level);
void wldcore_bar_restore_list_and_show_welcome(wldcore_menu_stack_record_t* level);
void wldcore_tutorial_restore_list_and_show_welcome(wldcore_menu_stack_record_t* level);
void wldcore_window_finalize_record_and_stop_task(wldcore_menu_stack_record_t* level);
void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image);
void wldcore_window_draw_centered_text(s32 window_index, s32 text_id);
void wldcore_window_init_number_panel_render_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin);
void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);
void wldcore_window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags);
void wldcore_window_load_image_record_to_vram(s32 index, u32* image);
void wldcore_window_publish_slot_b806(wldcore_menu_list_window_level_t* level);
void wldcore_window_publish_slot_b8dd(wldcore_menu_list_window_level_t* level);
void wldcore_window_reset_state_and_scroll(wldcore_menu_stack_record_t* level);
void wldcore_window_set_render_objects_visible(s32 clear);
void wldcore_window_set_render_palette_2(wldcore_window_render_pair_t* pair);
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin);
void wldcore_window_set_layout_states(wldcore_menu_proposition_list_level_t* level);
void wldcore_window_set_list_window_states(wldcore_menu_list_window_level_t* state);
void wldcore_window_end_tint_transition(wldcore_menu_list_window_level_t* level);
void wldcore_window_begin_tint_transition(wldcore_menu_list_window_level_t* level);
void wldcore_window_set_secondary_render_palette_2(wldcore_menu_slot_set_t* slots);
void wldcore_window_clear_states_and_restore_tint(wldcore_menu_send_unit_level_t* level);
void wldcore_window_set_states_and_blue_tint(wldcore_menu_send_unit_level_t* level);
void wldcore_window_reset_slot_state(wldcore_menu_save_confirm_level_t* level);

#endif

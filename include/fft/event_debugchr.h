#ifndef EVENT_DEBUGCHR_H
#define EVENT_DEBUGCHR_H

/* EVENT/DEBUGCHR.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"

/* editor */
extern s32 g_debugchr_editor_active;
extern help_navigation_record_t* g_debugchr_editor_current_field;
extern POLY_FT4 g_debugchr_editor_cursor_polys[];
extern POLY_FT4 g_debugchr_editor_cursor_shade_polys[];
extern s32 g_debugchr_editor_field_edit_active;
extern help_navigation_record_t* g_debugchr_editor_field_table;
extern help_navigation_record_t g_debugchr_editor_fields[];
extern s16 g_debugchr_editor_hp_max;
extern u8 g_debugchr_editor_job_name[];
extern u8 g_debugchr_editor_jump;
extern u8 g_debugchr_editor_move;
extern u8 g_debugchr_editor_primary_skillset_name[];
extern s16 g_debugchr_editor_mp_max;
extern u8 g_debugchr_editor_secondary_skillset_name[];
extern s32 g_debugchr_editor_selected_unit_slot;
extern u8 g_debugchr_editor_speed;
/* 0x801c5e34; per-row value ranges, copied from the _normal or _special table. */
extern ranges32_t g_debugchr_editor_template[21];
extern u8 g_debugchr_editor_template_normal[];
extern u8 g_debugchr_editor_template_special[];
extern s16 g_debugchr_editor_ct_max;
extern u8 g_debugchr_editor_unit_ids[];
extern u8 g_debugchr_editor_unit_name[];
/* Views of g_debugchr_panel_selected_billboard.gauges[i].value (values[3 * i])
 * and gauges[i].max (the *_max 16-bit values): debugchr_editor_apply_values_to_battle_unit
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_debugchr_editor_values[];

void debugchr_editor_apply_values_to_battle_unit(s32 unit_id);
void debugchr_editor_load_selected_unit(void);
void debugchr_editor_run_unit_thread(s32 unit_id);
void debugchr_editor_run_session(void);

/* character status */
extern menu_status_thread_t g_debugchr_character_status_frame_config;

/* unit panels */
extern menu_unit_status_record_t g_debugchr_panel_comparison_billboard;
extern menu_status_panel_editor_packet_t g_debugchr_panel_comparison_editor_packets[2];
extern u8 g_debugchr_panel_comparison_large_number_image[];
extern u8 g_debugchr_panel_comparison_name_image[];
extern u8 g_debugchr_panel_comparison_number_image[];
extern menu_status_panel_packet_t g_debugchr_panel_comparison_packets[2];
extern palette_t g_debugchr_panel_comparison_portrait_palettes[2];
extern RECT g_debugchr_panel_comparison_portrait_rect;
extern menu_active_unit_t g_debugchr_panel_comparison_unit_data;
extern image_load_parameters_t g_debugchr_panel_editor_label_cells[];
extern u16 g_debugchr_panel_editor_label_cluts[][2];
extern image_load_parameters_t g_debugchr_panel_editor_mode_cell[];
extern image_load_parameters_t g_debugchr_panel_editor_mode_cells[];
extern image_load_parameters_t g_debugchr_panel_editor_value_cells[];
extern u16 g_debugchr_panel_editor_value_cluts[][2];
extern RECT g_debugchr_panel_frame_rect;
extern CVECTOR g_debugchr_panel_gauge_bar_colors[];
extern image_load_parameters_t g_debugchr_panel_portrait_cell[];
extern menu_unit_status_record_t g_debugchr_panel_selected_billboard;
extern menu_status_panel_editor_packet_t g_debugchr_panel_selected_editor_packets[2];
extern u8 g_debugchr_panel_selected_large_number_image[];
extern u8 g_debugchr_panel_selected_name_image[];
extern u8 g_debugchr_panel_selected_number_image[];
extern menu_status_panel_packet_t g_debugchr_panel_selected_packets[2];
extern palette_t g_debugchr_panel_selected_portrait_palettes[2];
extern RECT g_debugchr_panel_selected_portrait_rect;
/* Selected-unit panel record at 0x801c5324; the comparison panel's twin
 * follows at 0x801c5334. */
extern menu_active_unit_t g_debugchr_panel_selected_unit_data;
extern s16 g_debugchr_panel_slide_down_y[];
extern s16 g_debugchr_panel_slide_up_y[];
extern image_load_parameters_t g_debugchr_panel_sprite_cells[];
extern menu_status_thread_t g_debugchr_status_display_thread_params;

void debugchr_panel_run_unit_status_thread(void);

/* numeric editor */
extern menu_number_entry_t g_debugchr_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_debugchr_editor_numeric_descriptor_b[];
extern RECT g_debugchr_editor_numeric_entries_a;
extern RECT g_debugchr_editor_numeric_entries_b;
extern RECT g_debugchr_editor_numeric_entries_c;
extern RECT g_debugchr_editor_numeric_entries_d;
extern RECT g_debugchr_editor_numeric_entries_e;
extern RECT g_debugchr_editor_numeric_entries_f;
extern menu_status_panel_numeric_buffer_t g_debugchr_editor_numeric_state_a[2];
extern menu_status_panel_numeric_buffer_t g_debugchr_editor_numeric_state_b[2];
extern battle_menu_status_panel_numeric_geometry_t g_debugchr_editor_numeric_table;
extern menu_unit_stat_number_images_t g_debugchr_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_debugchr_editor_numeric_text_b;
extern image_load_parameters_t g_debugchr_editor_numeric_texture[];
extern menu_status_thread_t g_debugchr_numeric_editor_thread_params;
extern menu_number_entry_t g_debugchr_panel_comparison_numeric_entries[];
extern menu_number_entry_t g_debugchr_panel_selected_numeric_entries[];

void debugchr_editor_run_numeric_thread(void);

/* menus */

/* graphics */
extern POLY_FT4 g_debugchr_gfx_render_buffers[2][30];

/* text */

/* input */

/* threads */

#endif

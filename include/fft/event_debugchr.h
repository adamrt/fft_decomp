#ifndef EVENT_DEBUGCHR_H
#define EVENT_DEBUGCHR_H

/* EVENT/DEBUGCHR.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
extern s16 g_debugchr_editor_mp_max;
extern s32 g_debugchr_editor_selected_unit_slot;
extern u8 g_debugchr_editor_speed;
/* 0x801c5e34; per-row value ranges, copied from the _normal or _special table. */
extern ranges32_t g_debugchr_editor_template[21];
extern u8 g_debugchr_editor_template_normal[];
extern u8 g_debugchr_editor_template_special[];
extern s16 g_debugchr_editor_ct_max;
extern u8 g_debugchr_editor_unit_ids[];
extern u8 g_debugchr_editor_unit_name[];
/* Views of g_panel_selected_billboard.gauges[i].value (values[3 * i])
 * and gauges[i].max (the *_max 16-bit values): debugchr_editor_apply_values_to_battle_unit
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_debugchr_editor_values[];

void debugchr_editor_run_unit_thread(s32 unit_id);
void debugchr_editor_run_session(void);

/* character status */
extern menu_status_thread_t g_debugchr_character_status_frame_config;

/* unit panels */
extern palette_t g_debugchr_panel_comparison_portrait_palettes[2];
extern menu_active_unit_t g_debugchr_panel_comparison_unit_data;
extern palette_t g_debugchr_panel_selected_portrait_palettes[2];
/* Selected-unit panel record at 0x801c5324; the comparison panel's twin
 * follows at 0x801c5334. */
extern menu_active_unit_t g_debugchr_panel_selected_unit_data;
extern menu_status_thread_t g_debugchr_status_display_thread_params;

/* numeric editor */
extern menu_status_thread_t g_debugchr_numeric_editor_thread_params;

void debugchr_editor_run_numeric_thread(void);

/* menus */

/* graphics */
extern POLY_FT4 g_debugchr_gfx_render_buffers[2][30];

/* text */

/* input */

/* threads */

#endif

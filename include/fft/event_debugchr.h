#ifndef EVENT_DEBUGCHR_H
#define EVENT_DEBUGCHR_H

/* EVENT/DEBUGCHR.OUT: the BATTLE overlay of the debug unit editor and the mounted unit's status screen. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_help.h"
#include "fft/menu_panel.h"
#include "fft/menu_status.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* editor */
extern s32 g_debugchr_editor_active;
extern u8 g_debugchr_editor_unit_ids[];
extern u8 g_debugchr_editor_unit_name[];
extern u8 g_debugchr_editor_job_name[];
extern help_navigation_record_t g_debugchr_editor_fields[];
extern s32 g_debugchr_editor_field_edit_active;
/* Per-row value ranges, copied from the _normal or _special table. */
extern ranges32_t g_debugchr_editor_template[21];
extern u8 g_debugchr_editor_template_normal[];
extern u8 g_debugchr_editor_template_special[];
/* Views of g_panel_selected_billboard.gauges[i].value (values[3 * i])
 * and gauges[i].max (the *_max 16-bit values): debugchr_editor_apply_values_to_battle_unit
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_debugchr_editor_values[];
extern s16 g_debugchr_editor_hp_max;
extern s16 g_debugchr_editor_mp_max;
extern s16 g_debugchr_editor_ct_max;
extern u8 g_debugchr_editor_move;
extern u8 g_debugchr_editor_jump;
extern u8 g_debugchr_editor_speed;

void debugchr_editor_run_session(void);
void debugchr_status_run_unit_thread(s32 unit_id);
void debugchr_editor_run_numeric_thread(void);

/* unit panels */
/* The selected-unit panel record; the comparison panel's record follows 0x10
 * bytes later. */
extern menu_active_unit_t g_debugchr_panel_selected_unit_data;
extern palette_t g_debugchr_panel_selected_portrait_palettes[2];
extern menu_active_unit_t g_debugchr_panel_comparison_unit_data;
extern palette_t g_debugchr_panel_comparison_portrait_palettes[2];
extern menu_status_thread_t g_debugchr_status_display_thread_params;
extern menu_status_thread_t g_debugchr_character_status_frame_config;
extern menu_status_thread_t g_debugchr_numeric_editor_thread_params;

/* graphics */

#endif

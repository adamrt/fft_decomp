#ifndef UNIT_EDITOR_H
#define UNIT_EDITOR_H

/* DEBUGCHR's debug unit editor: its field table, the per-field value ranges and the values it writes back to a battle
 * unit, and the event library's thread that opens it. */

#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern s32 g_unit_editor_active;
extern u8 g_unit_editor_unit_ids[];
extern u8 g_unit_editor_unit_name[];
extern u8 g_unit_editor_job_name[];
extern help_navigation_record_t g_unit_editor_fields[];
extern s32 g_unit_editor_field_edit_active;
extern u8 g_unit_editor_move;
extern u8 g_unit_editor_jump;
extern u8 g_unit_editor_speed;
extern unit_panel_thread_t g_unit_editor_status_display_thread_params;

/* Per-row value ranges, copied from the _normal or _special table. */
extern ranges32_t g_unit_editor_template[21];
extern u8 g_unit_editor_template_normal[];
extern u8 g_unit_editor_template_special[];

/* Views of g_unit_panel_selected_billboard.gauges[i].value (values[3 * i])
 * and gauges[i].max (the *_max 16-bit values): unit_editor_apply_values
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_unit_editor_values[];
extern s16 g_unit_editor_hp_max;
extern s16 g_unit_editor_mp_max;
extern s16 g_unit_editor_ct_max;

void unit_editor_run(void);

/* The event library's menu thread: closes the unit view panels, loads DEBUGCHR and runs unit_editor_run. */
void unit_editor_open(void);

#endif

#ifndef FORMATION_JOB_H
#define FORMATION_JOB_H

/* The Formation screen's job menu in WORLD: changing a unit's job, the job wheel and its job-change animation; the
 * ATTACK and REQUIRE overlays' party portrait job record sits here too. */

#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* job change */
s32 formation_job_change(s16 unit_id, s16 job_id, s32 update_equipment);
s32 formation_job_is_locked_for_all(s16 job_id);

/* job wheel */
/* An entry of the job list job_list_build writes. */
enum {
    FORMATION_JOB_LIST_ENTRY_ID_MASK = 0x3ff,
    /* Set in mode INCLUDE_OTHER_UNIT_UNLOCKS on the unit's current job and on
     * a job only another unit has unlocked: the job wheel shows the entry
     * but does not change to it. */
    FORMATION_JOB_LIST_ENTRY_UNAVAILABLE = 0x4000,
};

/* The one mode job_list_build tests; its callers also pass 0 and 1. */
enum { FORMATION_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS = 2 };

extern s8 g_formation_job_wheel_initialized;
extern s16 g_formation_job_wheel_unit_index;
extern s16 g_formation_job_wheel_jobs[];
extern s16 g_formation_job_wheel_mastered_jobs[];
extern s16 g_formation_job_wheel_job_count;
extern s16 g_formation_job_wheel_cursor_index;
extern u8 g_formation_job_wheel_job_selected;
extern u8 g_formation_job_wheel_saved_browse_enabled;
extern menu_instruction_t g_formation_job_wheel_script;
extern s16 g_formation_job_wheel_name_text;
extern RECT g_formation_job_wheel_name_rect;
extern oriented_quad_t g_formation_job_wheel_mastered_icon_sprite;
extern point16_t g_formation_job_wheel_unit_sprite_position;
extern u8 g_formation_job_wheel_center_x;
extern u8 g_formation_job_wheel_center_y;
extern u8 g_formation_job_wheel_animation_counter;
extern s8 g_formation_job_wheel_spin_progress;
extern s8 g_formation_job_wheel_spin_direction;
extern u8 g_formation_job_wheel_opening;
extern u8 g_formation_job_wheel_open_frame;
extern u8 g_formation_job_wheel_closing;
extern u8 g_formation_job_wheel_close_frame;

/* The current and the new job's portrait; separate objects, since
 * formation_job_wheel_menu addresses each with its own lui/addiu. */
extern textured_rect_source_t g_formation_job_wheel_portrait_quad;
extern textured_rect_source_t g_formation_job_wheel_new_portrait_quad;

void formation_job_wheel_menu(void);

/* job-change animation */
/* Result of formation_job_step_change, kept in
 * g_formation_job_change_anim_state: the phase of the job-change dissolve. */
typedef enum formation_job_change_anim {
    FORMATION_JOB_CHANGE_ANIM_DONE = 0,
    FORMATION_JOB_CHANGE_ANIM_DISSOLVE = 1,
    FORMATION_JOB_CHANGE_ANIM_CLOSING = 2, /* the last 20 frames */
} formation_job_change_anim_e;

extern u8 g_formation_job_change_active;
extern s8 g_formation_job_change_anim_state; /* formation_job_change_anim_e */
extern u8 g_formation_job_change_confirmed;
extern s16 g_formation_job_change_skip_requested;
extern s16 g_formation_job_change_frame;
extern rgb24_t g_formation_job_change_wave_colors[3];
extern rgb24_t g_formation_job_change_flash_colors[4];
extern s16* g_formation_job_change_dissolve_order;
extern RECT g_formation_job_change_old_sprite_rect;
extern RECT g_formation_job_change_new_sprite_rect;
extern u8* g_formation_job_change_old_sprite_image;
extern u8* g_formation_job_change_new_sprite_image;
extern u8* g_formation_job_change_new_sprite_source;

/* party portraits */
/* The job record the ATTACK and REQUIRE party portrait builders last looked
 * up; each overlay defines its own. */
extern job_data_t* g_formation_job_data;

#endif

#include "fft/world.h"
#include "psx/libgte.h"
#include "psx/types.h"

/*
 * Draw the job wheel's generic-job sprites around an ellipse.
 *
 * Jobs are placed backwards from the cursor at angle steps of ONE / count on
 * radii scaled by 100 and 60 (ONE = 1.0). The ordering-table index rises in
 * the first and last angle quarters and falls in the middle two. Jobs flagged
 * 0x4000 are tinted and those with a nonzero g_world_job_wheel_mastered_jobs entry get a level
 * gauge. Needs at least two jobs.
 */
void world_formation_draw_job_wheel_sprites(s32 angle, s32 radius_x, s32 radius_y) {
    oriented_quad_t sprite;
    u8 unused_sprite_tail[0x10]; /* Preserve the original 0x24-byte stack allocation. */
    s32 step;
    s32 ot_index;
    s32 i;
    u16 index;
    s32 scaled_x;
    s32 scaled_y;
    s32 theta;
    s16 job;
    rgb24_t* rgb;
    s16 next;

    if (g_world_job_wheel_job_count < 2) {
        return;
    }
    step = ONE / g_world_job_wheel_job_count;
    g_world_job_wheel_animation_counter++;
    ot_index = 30;
    index = g_world_job_wheel_cursor_index;
    sprite.w = sprite.source.w = 0x18;
    sprite.h = sprite.source.h = 0x28;
    g_world_job_wheel_animation_counter = g_world_job_wheel_animation_counter % 102;
    scaled_x = (radius_x * 100) >> 12;
    scaled_y = (radius_y * 60) >> 12;
    for (i = 0; i < g_world_job_wheel_job_count; i++) {
        theta = (i * step + 0x400 + angle) % ONE;
        if (theta < 0x400) {
            ot_index++;
        } else if (theta < 0x800) {
            ot_index--;
        } else if (theta < 0xC00) {
            ot_index--;
        } else {
            ot_index++;
        }
        job = index;
        world_formation_get_generic_human_graphic_entry(g_world_job_wheel_jobs[job] & WORLD_JOB_LIST_ENTRY_ID_MASK,
            g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_FEMALE, &sprite.source);
        sprite.position.x = g_world_job_wheel_center_x + ((scaled_x * rcos(theta)) >> 12) - 8;
        sprite.position.y = g_world_job_wheel_center_y + ((scaled_y * rsin(theta)) >> 12) - 0x14;
        rgb = 0;
        if (g_world_job_wheel_jobs[job] & WORLD_JOB_LIST_ENTRY_UNAVAILABLE) {
            rgb = &g_world_formation_dimmed_sprite_color;
        }
        if (g_world_job_wheel_mastered_jobs[job] != 0) {
            world_menu_select_stat_gauge_level(
                sprite.position.x, sprite.position.y, ot_index, g_world_job_wheel_animation_counter);
        }
        world_formation_draw_sprite_with_shadow(&sprite, rgb, ot_index);
        if (job == 0) {
            next = g_world_job_wheel_job_count - 1;
        } else {
            next = index - 1;
        }
        index = next;
    }
}

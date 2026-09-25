#include "fft/data.h"
#include "fft/world.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Job-wheel sprite record: destination rectangle followed by the 12-byte
 * source entry, as in world_formation_draw_unit_sprite. */
typedef struct world_formation_job_wheel_sprite {
    s16 x;                                  /* 0x00 */
    s16 y;                                  /* 0x02 */
    s16 w;                                  /* 0x04 */
    s16 h;                                  /* 0x06 */
    world_formation_graphic_entry_t source; /* 0x08 */
    u8 _pad14[0x10];                        /* unknown; the frame reserves 0x24 bytes for the record */
} world_formation_job_wheel_sprite_t;

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
    world_formation_job_wheel_sprite_t sprite;
    s32 step;
    s32 ot_index;
    s32 i;
    u16 index;
    s32 scaled_x;
    s32 scaled_y;
    s32 theta;
    s16 job;
    s8* rgb;
    s16 next;

    if (g_world_job_wheel_job_count < 2) {
        return;
    }
    step = ONE / g_world_job_wheel_job_count;
    g_world_job_wheel_animation_counter++;
    ot_index = 30;
    index = *(u16*)&g_world_job_wheel_cursor_index;
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
        world_formation_get_generic_human_graphic_entry(g_world_job_wheel_jobs[job] & 0x3FF,
            g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags & UNIT_FLAG_FEMALE,
            &sprite.source);
        sprite.x = g_world_job_wheel_center_x + ((scaled_x * rcos(theta)) >> 12) - 8;
        sprite.y = g_world_job_wheel_center_y + ((scaled_y * rsin(theta)) >> 12) - 0x14;
        rgb = 0;
        if (g_world_job_wheel_jobs[job] & 0x4000) {
            rgb = g_world_formation_dimmed_sprite_color;
        }
        if (g_world_job_wheel_mastered_jobs[job] != 0) {
            world_menu_select_stat_gauge_level(sprite.x, sprite.y, ot_index, g_world_job_wheel_animation_counter);
        }
        world_formation_draw_sprite_with_shadow((world_gfx_sprite_desc_t*)&sprite, (s32)rgb, ot_index);
        if (job == 0) {
            next = g_world_job_wheel_job_count - 1;
        } else {
            next = index - 1;
        }
        index = next;
    }
}

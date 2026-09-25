#include "fft/world.h"
#include "psx/types.h"

/* Render the job wheel's centred job name ("Can't change" when fewer than two
 * jobs are listed). */
void world_formation_build_job_wheel_job_name(void) {
    RECT destination_rect;
    u8* font;
    u16* name;
    s32 pixel_width;

    if (g_world_job_wheel_job_count < 2) {
        font = g_world_text_section_pointers[27];
        g_world_job_wheel_name_text = 0x1B;
    } else {
        font = g_world_text_job_names;
        g_world_job_wheel_name_text = g_world_job_wheel_jobs[g_world_job_wheel_cursor_index];
    }
    name = &g_world_job_wheel_name_text;
    destination_rect.x = 0x24C;
    destination_rect.y = 0x130;
    destination_rect.w = 0x14;
    destination_rect.h = 0x10;
    world_text_render_id_list_to_image_rows(font, (s16*)name, &destination_rect, 0);
    pixel_width = (s8)world_text_measure_entry_pixel_width(world_text_find_entry_by_index(font, *name & 0x3FF, 2));
    g_world_job_wheel_name_rect.x = ((0x50 - pixel_width) >> 1) + 0x58;
    g_world_job_wheel_name_rect.y = 0xD7;
    g_world_job_wheel_name_rect.w = pixel_width;
    g_world_job_wheel_name_rect.h = 0xE;
}

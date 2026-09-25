#include "fft/wldcore.h"
#include "psx/gpu.h"
#include "psx/gs.h"

/* Pushes menu level type 0x14 (driven by wldcore_menu_step_sound_novel_resume_level) with the given mode:
 * appends the cursor window, a VRAM render record for the TIM at g_wldcore_picture_buffer
 * (uploaded progressively through wldcore_gfx_step_dissolve_image_upload, starting at step 0), and
 * two text render records (0xb84c with substitution value 0xe000 + mode, and
 * 0xb84b). The picture record is centred against the first text record from
 * the TIM's width and half height, and the cursor window is placed at the
 * second text record's origin + (0x18, 0x3c).
 *
 * The target reserves 0x10 frame bytes the function never uses. The
 * two offset temporaries keep GCC from folding base_x - (width - 0x3e) into
 * (base_x + 0x3e) - width, and each render-record base pointer is its own
 * local so the pointer shares the register of the constant it dies into. */
void wldcore_menu_push_sound_novel_resume_level(s32 mode) {
    GsIMAGE image;
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t clut;
    s32 unused[4];
    s32 index;
    s32 depth;
    s32 offset_x;
    s32 offset_y;
    wldcore_point32_t* base1;
    wldcore_point32_t* base2;
    wldcore_point32_t* base3;
    wldcore_point32_t* base4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.mode = mode;
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.picture_render = index;
    bounds.position.x = image.px;
    bounds.position.y = image.py;
    bounds.dimensions.x = image.pw;
    bounds.dimensions.y = image.ph;
    clut.x = image.cx;
    clut.y = image.cy;
    wldcore_window_init_vram_render_record(index, bounds.position, bounds.dimensions, clut, 4);
    g_wldcore_window_render_records[index].priority = 0xA;
    wldcore_gfx_step_dissolve_image_upload(&image, 0);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.message_render = index;
    bounds.position.x = 0;
    bounds.position.y = 0x70;
    bounds.dimensions.x = 0x7C;
    bounds.dimensions.y = 0x80;
    g_world_text_substitution_values[0] = mode + 0xE000;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 1, 0xB84C, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    base1 = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base1->x = -0x70;
    base1->y = -0x38;
    DrawSync(0);
    depth = g_wldcore_menu_stack_depth;
    base2 = (wldcore_point32_t*)&g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth]
                                                                     .sound_novel_resume.picture_render]
                .base_x;
    offset_x = image.pw - 0x3E;
    base2->x = g_wldcore_window_render_records[index].base_x - offset_x;
    base3 = (wldcore_point32_t*)&g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth]
                                                                     .sound_novel_resume.picture_render]
                .base_x;
    offset_y = (image.ph >> 1) - 0x4C;
    base3->y = g_wldcore_window_render_records[index].base_y - offset_y;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.option_render = index;
    bounds.position.x = 0x80;
    bounds.position.y = 0x70;
    bounds.dimensions.x = 0x70;
    bounds.dimensions.y = 0x70;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 1, 0xB84B, g_wldcore_window_image_buffer + 0x400);
    g_wldcore_window_render_records[index].priority = 0xA;
    base4 = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base4->x = 8;
    base4->y = -0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .x = g_wldcore_window_render_records[index].base_x + 0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .y = g_wldcore_window_render_records[index].base_y + 0x3C;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.dissolve_step = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_row = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME;
    g_wldcore_menu_stack_depth++;
}

#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"

/* Pushes menu level type 0x14 (driven by wldcore_menu_step_sound_novel_resume_level) with the given mode:
 * appends the cursor window, a VRAM render record for the TIM at g_wldcore_picture_buffer
 * (uploaded progressively through wldcore_gfx_step_dissolve_image_upload, starting at step 0), and
 * two text render records (0xb84c with substitution value 0xe000 + mode, and
 * 0xb84b). The picture record is centerd against the first text record from
 * the TIM's width and half height, and the cursor window is placed at the
 * second text record's origin + (0x18, 0x3c).
 *
 * The target reserves 0x10 frame bytes the function never uses. The
 * two offset temporaries keep GCC from folding base_x - (width - 0x3e) into
 * (base_x + 0x3e) - width, and each render-record base pointer is its own
 * local so the pointer shares the register of the constant it dies into. */
void wldcore_menu_push_sound_novel_resume_level(s32 mode) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 unused[4];
    s32 index;
    s32 depth;
    s32 offset_x;
    s32 offset_y;
    point32_t* base1;
    point32_t* base2;
    point32_t* base3;
    point32_t* base4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.mode = mode;
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.picture_render = index;
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut.x = image.cx;
    clut.y = image.cy;
    wldcore_window_init_vram_render_record(index, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
    g_wldcore_window_render_records[index].priority = 0xA;
    wldcore_gfx_step_dissolve_image_upload(&image, 0);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.message_render = index;
    bounds.x = 0;
    bounds.y = 0x70;
    bounds.w = 0x7C;
    bounds.h = 0x80;
    g_text_substitution_values[0] = mode + TEXT_ID_TREASURE_NAME_BASE;
    wldcore_window_init_render_record_image(
        index, bounds, 1, WLDCORE_TEXT_ID_SUBSTITUTED_TEXT, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    base1 = &g_wldcore_window_render_records[index].base;
    base1->x = -0x70;
    base1->y = -0x38;
    DrawSync(0);
    depth = g_wldcore_menu_stack_depth;
    base2 = &g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].sound_novel_resume.picture_render]
                 .base;
    offset_x = image.pw - 0x3E;
    base2->x = g_wldcore_window_render_records[index].base.x - offset_x;
    base3 = &g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].sound_novel_resume.picture_render]
                 .base;
    offset_y = (image.ph >> 1) - 0x4C;
    base3->y = g_wldcore_window_render_records[index].base.y - offset_y;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.option_render = index;
    bounds.x = 0x80;
    bounds.y = 0x70;
    bounds.w = 0x70;
    bounds.h = 0x70;
    wldcore_window_init_render_record_image(
        index, bounds, 1, WLDCORE_TEXT_ID_SOUND_NOVEL_RESUME_CHOICES, g_wldcore_window_image_buffer + 0x400);
    g_wldcore_window_render_records[index].priority = 0xA;
    base4 = &g_wldcore_window_render_records[index].base;
    base4->x = 8;
    base4->y = -0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .position.y = g_wldcore_window_render_records[index].base.y + 0x3C;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.dissolve_step = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_row = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME;
    g_wldcore_menu_stack_depth++;
}

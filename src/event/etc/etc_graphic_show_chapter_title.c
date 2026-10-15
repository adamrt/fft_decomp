#include "fft/event_etc.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Show a chapter title graphic: wipe it in across its width one column per
 * event-speed step, hold it for 80 steps, then fade its brightness from 0x80
 * to 0 over 128 steps. Each frame builds the graphic twice, into the two
 * primitive sets (parts 2 and 0), alternating buffers by frame.
 */
void etc_graphic_show_chapter_title(s32 graphic_id) {
    /* The target reserves two otherwise unreferenced local 32-bit values. */
    s32 reserved_stack_words[2];
    s32 frame;
    s32 fade;
    s32 color;
    sprite_sheet_frame_t* dimensions;

    dimensions = g_etc_graphics[graphic_id].dimensions;
    frame = 0;
    fade = 0;
    if (dimensions[2].source.w > 0) {
        do {
            etc_graphic_build_chapter_polygons(
                graphic_id, fade, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
            etc_graphic_build_chapter_polygons(
                graphic_id, fade, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], 0x80);
            frame++;
            thread_yield();
            fade += g_script_event_speed;
        } while (fade < dimensions[2].source.w);
    }

    fade = 0;
    do {
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].source.w, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].source.w, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], 0x80);
        frame++;
        thread_yield();
        fade += g_script_event_speed;
    } while (fade < 0x50);

    color = 0x80;
    fade = 0;
    do {
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].source.w, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], color);
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].source.w, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], color);
        color -= g_script_event_speed;
        thread_yield();
        fade += g_script_event_speed;
        frame++;
    } while (fade < 0x80);
}

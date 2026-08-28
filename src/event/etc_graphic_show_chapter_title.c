#include "fft/etc.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void etc_graphic_show_chapter_title(s32 graphic_id) {
    /* The target reserves two otherwise unreferenced local words. */
    volatile s32 reserved_stack_words[2];
    s32 frame;
    s32 fade;
    s32 color;
    etc_graphic_dimensions_t* dimensions;

    dimensions = g_etc_graphics[graphic_id].dimensions;
    frame = 0;
    fade = 0;
    if (dimensions[2].width.signed_value > 0) {
        do {
            etc_graphic_build_chapter_polygons(
                graphic_id, fade, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
            etc_graphic_build_chapter_polygons(
                graphic_id, fade, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], 0x80);
            frame++;
            battle_thread_yield();
            fade += g_battle_event_speed;
        } while (fade < dimensions[2].width.signed_value);
    }

    fade = 0;
    do {
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].width.signed_value, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].width.signed_value, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], 0x80);
        frame++;
        battle_thread_yield();
        fade += g_battle_event_speed;
    } while (fade < 0x50);

    color = 0x80;
    fade = 0;
    do {
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].width.signed_value, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], color);
        etc_graphic_build_chapter_polygons(
            graphic_id, dimensions[2].width.signed_value, 0, 1, g_etc_graphic_chapter_primitives_b[frame & 1], color);
        color -= g_battle_event_speed;
        battle_thread_yield();
        fade += g_battle_event_speed;
        frame++;
    } while (fade < 0x80);
}

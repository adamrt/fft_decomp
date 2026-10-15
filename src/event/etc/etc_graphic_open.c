#include "fft/battle_gfx.h"
#include "fft/event_etc.h"
#include "fft/gfx.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Load ETC graphic `graphic_id`: upload it straight to VRAM, or for the full-screen
 * modes fade it in, hold it for a per-graphic number of frames and fade out. */
void etc_graphic_open(s32 graphic_id) {
    u8* image_buffer;
    s32 frame;

    while (1) {
        image_buffer = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x20000);
        if (image_buffer == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_etc_allocation_wait_message);
        }
    }

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (
        thread_call_on_main_stack(g_etc_graphics[graphic_id].lba, g_etc_graphics[graphic_id].size, image_buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    if (g_etc_graphics[graphic_id].texture_mode < 2U) {
        LoadImage(g_etc_graphics[graphic_id].framebuffer_rect, (u32*)image_buffer);
        if (g_etc_graphics[graphic_id].texture_mode == 1) {
            LoadImage(
                &g_etc_graphic_game_over_palette_rect, (u32*)(image_buffer + g_etc_graphics[graphic_id].size - 0x200));
        }
    } else {
        g_gfx_draw_primitives_immediately = 1;
        if (graphic_id == ETC_GRAPHIC_END1) {
            g_gfx_screen_fade = -0xff;
            do {
                thread_yield();
                g_gfx_screen_fade += 8;
            } while (g_gfx_screen_fade <= 0);
        } else {
            g_gfx_screen_fade = 0xff;
            do {
                thread_yield();
                g_gfx_screen_fade -= 8;
            } while (g_gfx_screen_fade > 0);
        }
        g_gfx_screen_fade = 0;

        if (graphic_id == ETC_GRAPHIC_END1) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 0x4ec);
        } else if (graphic_id == ETC_GRAPHIC_END3) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 0x5a0);
        } else if (graphic_id == ETC_GRAPHIC_END4) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 0x8ac);
        } else {
            frame = 0;
            if (graphic_id == ETC_GRAPHIC_END5) {
                do {
                    frame++;
                    thread_yield();
                    g_threads[g_thread_current_id].function_parameter_4 = 1;
                } while (frame < 0x258);
            } else {
                do {
                    frame++;
                    thread_yield();
                    g_threads[g_thread_current_id].function_parameter_4 = 1;
                } while (frame < 0x17e8);
            }
        }

        g_gfx_screen_fade = 0;
        do {
            thread_yield();
            thread_yield();
            g_gfx_screen_fade += 8;
        } while (g_gfx_screen_fade < 0x100);
        g_gfx_screen_fade = 0xff;
        g_gfx_draw_primitives_immediately = 0;
        if (graphic_id == ETC_GRAPHIC_END3) {
            g_gfx_screen_fade = 0;
        }
    }

    DrawSync(0);
    main_heap_free(image_buffer);
}

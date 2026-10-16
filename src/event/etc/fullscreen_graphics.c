/* Fullscreen graphics for the chapter title and game over screens. */

#include "fft/event_etc.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_sound.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Wait for the high overlay area, read graphic `graphic_id` into it and upload the image:
 * texture mode 0 (the 4-bit chapter titles) as is, mode 1 (the 8-bit game-over screen)
 * with the 256-color palette that ends its file. The END graphics (mode 2) upload
 * nothing: they fade the screen in, hold for a per-graphic number of frames and fade
 * out. The buffer is freed after. */
static void etc_graphic_open(s32 graphic_id) {
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

        /* Hold: 1260 frames for END1, 1440 for END3, 2220 for END4, 600 for END5 and 6120 for END2. */
        if (graphic_id == ETC_GRAPHIC_END1) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 1260);
        } else if (graphic_id == ETC_GRAPHIC_END3) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 1440);
        } else if (graphic_id == ETC_GRAPHIC_END4) {
            frame = 0;
            do {
                frame++;
                thread_yield();
                g_threads[g_thread_current_id].function_parameter_4 = 1;
            } while (frame < 2220);
        } else {
            frame = 0;
            if (graphic_id == ETC_GRAPHIC_END5) {
                do {
                    frame++;
                    thread_yield();
                    g_threads[g_thread_current_id].function_parameter_4 = 1;
                } while (frame < 600);
            } else {
                do {
                    frame++;
                    thread_yield();
                    g_threads[g_thread_current_id].function_parameter_4 = 1;
                } while (frame < 6120);
            }
        }

        /* Fade out at half the fade-in speed. */
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

/* Empty, and nothing on the disc calls it. */
void etc_graphic_noop_1(void) {
}

/* The record for one half of the title: `dimension_set` picks the pair,
 * `column` the half. */
#define DIM (dimensions + dimension_set)[column]

/* Build the 2x2 POLY_GT4 grid that shows a chapter title graphic. Row 0 is
 * the part already revealed by `fade`; row 1 is the 32-pixel gradient edge. */
static void etc_graphic_build_chapter_polygons(
    s32 graphic_id, s32 fade, s32 dimension_set, s32 layer, POLY_GT4* primitives, s32 color) {
    sprite_sheet_frame_t* dimensions;
    s32 texture_mode;
    POLY_GT4* poly;
    s32 row;
    s32 column;
    s32 clip;

    dimensions = g_etc_graphics[graphic_id].dimensions;
    texture_mode = g_etc_graphics[graphic_id].texture_mode;
    poly = primitives;
    for (row = 0; row < 2; row++) {
        for (column = 0; column < 2; column++) {
            SetPolyGT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 1);
            setUV4(poly, 0, 0, 1, 0, 0, 1, 1, 1);
            setXY4(poly, 0, 0, 0, 0, 0, 0, 0, 0);
            if (column != 0) {
                poly->tpage = getTPage(texture_mode, 1, 0x180, 0);
            } else {
                poly->tpage = getTPage(texture_mode, 2, 0x180, 0);
            }
            if (texture_mode == 0) {
                poly->clut = 0xFDB;
            } else {
                poly->clut = 0x7F80;
            }
            if (row == 0) {
                if (layer != 0) {
                    setUV4(poly, DIM.source.x + DIM.source.w - fade, DIM.source.y, DIM.source.x + DIM.source.w,
                        DIM.source.y, DIM.source.x + DIM.source.w - fade, DIM.source.y + DIM.source.h,
                        DIM.source.x + DIM.source.w, DIM.source.y + DIM.source.h);
                    setXY4(poly, DIM.offset.x + DIM.source.w - fade + 0x100, DIM.offset.y + 0x80,
                        DIM.offset.x + DIM.source.w + 0x100, DIM.offset.y + 0x80,
                        DIM.offset.x + DIM.source.w - fade + 0x100, DIM.offset.y + 0x80 + DIM.source.h,
                        DIM.offset.x + DIM.source.w + 0x100, DIM.offset.y + 0x80 + DIM.source.h);
                    setRGB0(poly, color, color, color);
                    setRGB1(poly, color, color, color);
                    setRGB2(poly, color, color, color);
                    setRGB3(poly, color, color, color);
                } else {
                    setUVWH(poly, DIM.source.x, DIM.source.y, fade, DIM.source.h);
                    setXYWH(poly, DIM.offset.x + 0x100, DIM.offset.y + 0x80, fade, DIM.source.h);
                    setRGB0(poly, color, color, color);
                    setRGB1(poly, color, color, color);
                    setRGB2(poly, color, color, color);
                    setRGB3(poly, color, color, color);
                }
            } else if (row == 1) {
                clip = (DIM.source.w < fade + 0x20) ? DIM.source.w - fade : 0x20;
                if (layer != 0) {
                    setUV4(poly, DIM.source.x + DIM.source.w - fade - clip, DIM.source.y,
                        DIM.source.x + DIM.source.w - fade, DIM.source.y, DIM.source.x + DIM.source.w - fade - clip,
                        DIM.source.y + DIM.source.h, DIM.source.x + DIM.source.w - fade, DIM.source.y + DIM.source.h);
                    setXYWH(poly, DIM.offset.x + DIM.source.w - fade + 0x100 - clip, DIM.offset.y + 0x80, clip,
                        DIM.source.h);
                    setRGB0(poly, 0, 0, 0);
                    setRGB1(poly, color, color, color);
                    setRGB2(poly, 0, 0, 0);
                    setRGB3(poly, color, color, color);
                } else {
                    setUVWH(poly, DIM.source.x + fade, DIM.source.y, clip, DIM.source.h);
                    setXYWH(poly, DIM.offset.x + fade + 0x100, DIM.offset.y + 0x80, clip, DIM.source.h);
                    setRGB0(poly, color, color, color);
                    setRGB1(poly, 0, 0, 0);
                    setRGB2(poly, color, color, color);
                    setRGB3(poly, 0, 0, 0);
                }
            }
            poly++;
        }
    }
    if (layer != 0) {
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
    }
}

/* The dimension row of the tile being built. Spelled out at each access: the
 * target forms this address inside every memory reference (scaled terms
 * first), which a row pointer variable does not. */
#define ROW (dimensions + dimension_set + tile)

/* Build and draw the four POLY_GT4 tiles of an ETC graphic revealed up to `fade`
 * pixels, followed by a gradient edge of up to 32 pixels.
 *
 * The graphic is always 8-bit (texture mode 1). Retail keeps the first
 * half's right edge in the dead `graphic_id` argument. The unsigned vertical
 * origin preserves the width/x load order. */
static void etc_graphic_build_game_over_polygons(
    s32 graphic_id, s32 fade, s32 dimension_set, s32 reverse_order, POLY_GT4* primitives, s32 color) {
    sprite_sheet_frame_t* dimensions;
    POLY_GT4* primitive;
    s32 texture_mode;
    s32 half;
    s32 tile;
    s32 clip;
    s32 fade_x;

    texture_mode = 1;
    dimensions = g_etc_graphics[graphic_id].dimensions;
    primitive = primitives;
    for (half = 0; half < 2; half++) {
        for (tile = 0; tile < 2; tile++) {
            SetPolyGT4(primitive);
            SetShadeTex(primitive, 0);
            SetSemiTrans(primitive, 1);
            setUV4(primitive, 0, 0, 1, 0, 0, 1, 1, 1);
            setXY4(primitive, 0, 0, 0, 0, 0, 0, 0, 0);
            if (tile != 0) {
                primitive->tpage = getTPage(texture_mode, 1, 384, 0);
            } else {
                primitive->tpage = getTPage(texture_mode, 2, 384, 0);
            }
            if (texture_mode != 0) {
                primitive->clut = 0x7f80;
            } else {
                primitive->clut = 0xfdb;
            }

            if (half == 0) {
                if (reverse_order == 0) {
                    primitive->u0 = ROW->source.x + ROW->source.w;
                    primitive->v0 = ROW->source.y;
                    primitive->u2 = ROW->source.x;
                    primitive->v2 = ROW->source.y;
                    primitive->u1 = ROW->source.x + ROW->source.w;
                    primitive->v1 = ROW->source.y + fade;
                    primitive->u3 = ROW->source.x;
                    primitive->v3 = ROW->source.y + fade;
                    primitive->x0 = ROW->offset.y + 0x100;
                    primitive->y0 = ROW->offset.x + 0x80;
                    graphic_id = fade + 0x100;
                    primitive->x1 = ROW->offset.y + graphic_id;
                    primitive->y1 = ROW->offset.x + 0x80;
                    primitive->x2 = ROW->offset.y + 0x100;
                    primitive->y2 = ROW->offset.x + (ROW->source.w + 0x80U);
                    primitive->x3 = ROW->offset.y + graphic_id;
                    primitive->y3 = ROW->offset.x + (ROW->source.w + 0x80U);
                    setRGB0(primitive, color, color, color);
                    setRGB1(primitive, color, color, color);
                    setRGB2(primitive, color, color, color);
                    setRGB3(primitive, color, color, color);
                }
            } else if (half == 1) {
                clip = ROW->source.h < fade + 0x20 ? ROW->source.h - fade : 0x20;
                if (reverse_order == 0) {
                    fade_x = fade + 0x100;
                    primitive->u0 = ROW->source.x + ROW->source.w;
                    primitive->v0 = ROW->source.y + fade;
                    primitive->u2 = ROW->source.x;
                    primitive->v2 = ROW->source.y + fade;
                    primitive->u1 = ROW->source.x + ROW->source.w;
                    primitive->v1 = ROW->source.y + fade + clip;
                    primitive->u3 = ROW->source.x;
                    primitive->v3 = ROW->source.y + fade + clip;
                    setXY4(primitive, ROW->offset.y + fade_x, ROW->offset.x + 0x80, ROW->offset.y + fade_x + clip,
                        ROW->offset.x + 0x80, ROW->offset.y + fade_x, ROW->offset.x + (ROW->source.w + 0x80U),
                        ROW->offset.y + fade_x + clip, ROW->offset.x + (ROW->source.w + 0x80U));
                    setRGB0(primitive, color, color, color);
                    setRGB1(primitive, 0, 0, 0);
                    setRGB2(primitive, color, color, color);
                    setRGB3(primitive, 0, 0, 0);
                }
            }
            primitive++;
        }
    }

    if (reverse_order != 0) {
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&primitives[3]);
        gfx_draw_or_append_gpu_primitive(&primitives[2]);
        gfx_draw_or_append_gpu_primitive(&primitives[1]);
        gfx_draw_or_append_gpu_primitive(&primitives[0]);
    }
}

/*
 * Show a chapter title graphic: wipe it in across its width one column per
 * event-speed step, hold it for 80 steps, then fade its brightness from 0x80
 * to 0 over 128 steps. Each frame builds the graphic twice, into the two
 * primitive sets (parts 2 and 0), alternating buffers by frame.
 */
static void etc_graphic_show_chapter_title(s32 graphic_id) {
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

/*
 * Show the game-over graphic: swap the scenario music for the game-over
 * track, reveal the graphic from the left, one pixel column (one row of the
 * rotated texture) per event-speed step, hold it for 600 steps, then fade its
 * brightness from 0x80 to 0 over 128 steps. The two primitive buffers
 * alternate by frame.
 */
static void etc_graphic_show_game_over(s32 graphic_id) {
    /* The target reserves two otherwise unreferenced local 32-bit values. */
    s32 reserved_stack_words[2];
    s32 frame;
    s32 fade;
    s32 color;
    sprite_sheet_frame_t* dimensions;

    g_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    thread_call_on_main_stack();
    g_thread_call_target = (void (*)(void))main_sound_start_music_load_into_free_slot;
    thread_call_on_main_stack(MAIN_MUSIC_GAME_OVER);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (thread_call_on_main_stack() != 0);
    g_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    thread_call_on_main_stack(1, 0x7f, 0);

    dimensions = g_etc_graphics[graphic_id].dimensions;
    frame = 0;
    fade = 0;
    if (dimensions[2].source.h > 0) {
        do {
            etc_graphic_build_game_over_polygons(
                graphic_id, fade, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
            frame++;
            thread_yield();
            fade += g_script_event_speed;
        } while (fade < dimensions[2].source.h);
    }

    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].source.h, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], 0x80);
        frame++;
        thread_yield();
        fade += g_script_event_speed;
    } while (fade < 0x258);

    color = 0x80;
    fade = 0;
    do {
        etc_graphic_build_game_over_polygons(
            graphic_id, dimensions[2].source.h, 2, 0, g_etc_graphic_chapter_primitives_a[frame & 1], color);
        color -= g_script_event_speed;
        thread_yield();
        fade += g_script_event_speed;
        frame++;
    } while (fade < 0x80);
}

/* Thread body: load ETC graphic parameter 1 and run its show routine, the
 * chapter title for texture mode 0 or the game-over screen for mode 1. */
void etc_graphic_show_async(void) {
    s32 graphic_id = thread_get_current_parameter_1();
    s32 graphic_type;

    etc_graphic_open(graphic_id);
    graphic_type = g_etc_graphics[graphic_id].texture_mode;
    if (graphic_type == 0) {
        etc_graphic_show_chapter_title(graphic_id);
    } else if (graphic_type == 1) {
        etc_graphic_show_game_over(graphic_id);
    }
    thread_exit_current();
}

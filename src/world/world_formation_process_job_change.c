#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run one frame of the WORLD job-change graphic dissolve; returns 1 while it
 * runs, 2 during the closing frames and 0 once finished.
 *
 * The first call changes unit 0x14 to `job`, copies the VRAM strip at
 * g_world_job_change_old_sprite_rect and the unit's new graphic into heap buffers, and shuffles a
 * 480-entry byte-index table. Each frame clears two shuffled bytes of the old
 * strip, copies the same bytes of the new graphic into an initially blank
 * strip at g_world_job_change_new_sprite_rect, uploads both, and draws a 64-line rcos/rsin wave band
 * that slides in over frames 0-19 and out over 221-239.
 * Input bits 0x60 skip ahead to frame 220; frame 240 frees the buffers.
 *
 * `j`/`k`/`n` are each reused across the phases, and the wave phase is
 * `i * k` with `k` holding the step: splitting any of them, or keeping a
 * separate phase accumulator, changes the target's register assignment.
 */
s32 world_formation_process_job_change(s32 job) {
    union {
        world_formation_graphic_entry_t entry;
        s16 points[6];
    } scratch;
    RECT rect;
    s32 i;
    s32 j;
    s32 k;
    s32 n;
    s32 swing;
    s32 amplitude;
    s32 offset;
    s32 line_y;
    s32 base;
    s32 line_x;

    if (g_world_job_change_active == 0) {
        main_sound_play_tune(5);
        g_world_job_change_active = 1;
        g_world_job_change_frame = 0;
        g_world_job_change_skip_requested = 0;
        world_input_clear_state();
        g_world_job_change_old_sprite_image = main_heap_alloc(0x1e0);
        world_gfx_move_image_to_ram_from_vram_and_wait(
            &g_world_job_change_old_sprite_rect, (u32*)g_world_job_change_old_sprite_image);
        g_world_job_change_new_sprite_image = main_heap_alloc(0x1e0);
        memset(g_world_job_change_new_sprite_image, 0, 0x1e0);
        world_formation_change_unit_job(0x14, job, 0);
        world_formation_build_unit_graphic_entry(0x14, &scratch.entry, 0);
        g_world_job_change_new_sprite_source = main_heap_alloc(0x1e0);
        if (scratch.entry.tpage == 100) {
            rect.x = ((s16)scratch.entry.x >> 2) + 0x100;
        } else {
            rect.x = ((s16)scratch.entry.x >> 2) + 0x140;
        }
        rect.y = scratch.entry.y;
        rect.w = (s16)scratch.entry.w >> 2;
        rect.h = scratch.entry.h;
        g_world_job_wheel_new_portrait_quad.clut = scratch.entry.clut;
        world_gfx_move_image_to_ram_from_vram_and_wait(&rect, (u32*)g_world_job_change_new_sprite_source);
        g_world_job_change_dissolve_order = main_heap_alloc(0x3c0);
        for (i = 479; i >= 0; i--) {
            g_world_job_change_dissolve_order[i] = i;
        }
        for (i = 0; i < 300; i++) {
            j = rand() % 480;
            k = rand() % 480;
            n = g_world_job_change_dissolve_order[j];
            g_world_job_change_dissolve_order[j] = g_world_job_change_dissolve_order[k];
            g_world_job_change_dissolve_order[k] = n;
        }
    }
    for (i = 0; i < 2; i++) {
        n = g_world_job_change_dissolve_order[g_world_job_change_frame * 2 + i];
        g_world_job_change_old_sprite_image[n] = 0;
        g_world_job_change_new_sprite_image[n] = g_world_job_change_new_sprite_source[n];
    }
    LoadImage(&g_world_job_change_old_sprite_rect, (u32*)g_world_job_change_old_sprite_image);
    LoadImage(&g_world_job_change_new_sprite_rect, (u32*)g_world_job_change_new_sprite_image);
    k = 64;
    amplitude = 20;
    offset = 0;
    swing = 10;
    if (g_world_job_change_frame < 20) {
        amplitude = g_world_job_change_frame;
        offset = (20 - amplitude) * 8;
    } else if (g_world_job_change_frame > 220) {
        amplitude = 240 - g_world_job_change_frame;
        offset = (g_world_job_change_frame - 220) * 8;
    }
    scratch.points[1] = 0;
    for (i = 0; i < 64; i++) {
        n = (i * k + g_world_job_change_frame * 4) % ONE;
        line_x = ((amplitude * rcos(n)) >> 12) + 0x80;
        scratch.points[0] = line_x;
        scratch.points[2] = line_x;
        scratch.points[4] = line_x;
        base = offset - 146;
        line_y = ((swing * rsin(n)) >> 12) - base;
        scratch.points[5] = scratch.points[3] = base = line_y;
        line_y -= 64;
        scratch.points[3] = line_y - offset;
        world_menu_add_gradient_line_primitive(&scratch.points[0], g_world_job_change_wave_colors, 1, 0x38);
        scratch.points[3]++;
        world_menu_add_gradient_line_primitive(&scratch.points[2], g_world_job_change_wave_colors + 3, 1, 0x38);
    }
    world_gfx_add_draw_mode_primitive(0, 0, GetTPage(0, 3, 0x100, 0), 0, 0x37);
    g_world_job_change_frame++;
    if (g_world_job_change_skip_requested != 0) {
        if (g_world_job_change_frame >= 40 && g_world_job_change_frame <= 220) {
            g_world_job_change_frame = 220;
            main_sound_set_tune_volume(0x46);
        }
    } else if (g_world_input_newly_pressed & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
        g_world_job_change_skip_requested = 1;
    }
    if (g_world_job_change_frame < 220) {
        return 1;
    }
    if (g_world_job_change_frame < 240) {
        return 2;
    }
    main_heap_free(g_world_job_change_old_sprite_image);
    main_heap_free(g_world_job_change_new_sprite_image);
    main_heap_free(g_world_job_change_new_sprite_source);
    main_heap_free(g_world_job_change_dissolve_order);
    g_world_job_change_active = 0;
    return 0;
}

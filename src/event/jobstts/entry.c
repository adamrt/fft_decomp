/* Image loading, selection reset and the JOBSTTS entrypoint. */

#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_overlay.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Upload the menu glyph sheet and palette image, copy two window-frame palettes into VRAM row 254
 * (each also as a 12-color copy from color 4, with color 4 cleared), and clear the menu
 * selections. */
static void jobstts_menu_load_images_and_clear_selection(void) {
    RECT rect;
    palette_t palette;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_jobstts_menu_palette_image);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);

    menu_clear_selection_values();
    g_text_selection_id = 0;
}

/* Per-frame JOBSTTS entry: initialize on first call, run the active page,
 * and return the ordering table to draw (0 once the screen closes). */
u32* jobstts_entrypoint(s32 unit, void* unused_otag) {
    u32* result;

    if (g_jobstts_initialized == 0) {
        g_gfx_otag_length = 0x40;
        g_gfx_poly_ft4_capacity = 0x1f4;
        g_jobstts_gfx_draw_move_capacity = 0x14;
        g_jobstts_gfx_draw_area_capacity = 0xa;
        g_gfx_poly_f3_capacity = 0;
        g_gfx_poly_ft3_capacity = 0;
        g_gfx_poly_f4_capacity = 0;
        g_gfx_poly_g3_capacity = 0;
        g_gfx_poly_gt3_capacity = 0;
        g_gfx_poly_g4_capacity = 0;
        g_gfx_poly_gt4_capacity = 0;
        g_gfx_line_f2_capacity = 0;
        g_gfx_line_f3_capacity = 0;
        g_gfx_line_f4_capacity = 0;
        g_jobstts_gfx_line_g2_capacity = 0;
        g_jobstts_gfx_line_g3_capacity = 0;
        g_gfx_line_g4_capacity = 0;
        g_gfx_tile_capacity = 0;
        g_gfx_tile_1_capacity = 0;
        g_gfx_tile_8_capacity = 0;
        g_gfx_tile_16_capacity = 0;
        g_gfx_sprite_capacity = 0;
        g_gfx_sprite_8_capacity = 0;
        g_gfx_sprite_16_capacity = 0;
        jobstts_gfx_init_contexts(g_jobstts_gfx_contexts, g_gfx_otag_buffer, 0, 0, 0, g_gfx_poly_ft4_buffer, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, g_gfx_draw_move_buffer, g_gfx_draw_area_buffer);
        jobstts_menu_load_images_and_clear_selection();
        jobstts_unit_init_job_data(unit);
        menu_clear_selection_records();
        g_jobstts_initialized = 1;
    }
    jobstts_gfx_swap_context_and_clear_otag(0, -1);
    result = g_menu_gfx_context->otag;
    g_jobstts_previous_event_mode = g_menu_input_lock_mode;
    jobstts_menu_input_update_with_message_state();
    if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
        menu_text_start_help_thread(3);
    }
    if (g_jobstts_previous_event_mode != 0 && g_menu_input_lock_mode == 0) {
        text_restore_section_pointers();
    }
    menu_set_palette_mode(g_menu_input_lock_mode);
    if (g_jobstts_active_page == MENU_JOB_LIST_STEP_JOBS) {
        g_jobstts_active_page = jobstts_menu_run_job_list();
        if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
            result = 0;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_jobstts_initialized = 0;
        }
    } else if (g_jobstts_active_page == MENU_JOB_LIST_STEP_ABILITIES) {
        g_jobstts_active_page = jobstts_menu_ability_update_list_menu();
    } else {
        result = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        g_jobstts_initialized = 0;
    }
    if (g_sound_queued_effect_id != MAIN_SFX_NONE) {
        main_sound_play_sfx(g_sound_queued_effect_id);
        g_sound_queued_effect_id = MAIN_SFX_NONE;
    }
    return result;
}

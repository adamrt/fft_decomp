#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame JOBSTTS entry: initialize on first call, run the active page,
 * and return the ordering table to draw (0 once the screen closes). */
u32* jobstts_entrypoint(s32 unit, void* unused_otag) {
    u32* result;

    if (g_jobstts_initialized == 0) {
        g_jobstts_gfx_otag_length = 0x40;
        g_jobstts_gfx_poly_ft4_capacity = 0x1f4;
        g_jobstts_gfx_draw_move_capacity = 0x14;
        g_jobstts_gfx_draw_area_capacity = 0xa;
        g_jobstts_gfx_poly_f3_capacity = 0;
        g_jobstts_gfx_poly_ft3_capacity = 0;
        g_jobstts_gfx_poly_f4_capacity = 0;
        g_jobstts_gfx_poly_g3_capacity = 0;
        g_jobstts_gfx_poly_gt3_capacity = 0;
        g_jobstts_gfx_poly_g4_capacity = 0;
        g_jobstts_gfx_poly_gt4_capacity = 0;
        g_jobstts_gfx_line_f2_capacity = 0;
        g_jobstts_gfx_line_f3_capacity = 0;
        g_jobstts_gfx_line_f4_capacity = 0;
        g_jobstts_gfx_line_g2_capacity = 0;
        g_jobstts_gfx_line_g3_capacity = 0;
        g_jobstts_gfx_line_g4_capacity = 0;
        g_jobstts_gfx_tile_capacity = 0;
        g_jobstts_gfx_tile_1_capacity = 0;
        g_jobstts_gfx_tile_8_capacity = 0;
        g_jobstts_gfx_tile_16_capacity = 0;
        g_jobstts_gfx_sprite_capacity = 0;
        g_jobstts_gfx_sprite_8_capacity = 0;
        g_jobstts_gfx_sprite_16_capacity = 0;
        jobstts_gfx_init_contexts(g_jobstts_gfx_contexts, g_jobstts_gfx_otag_buffer, 0, 0, 0,
            g_jobstts_gfx_poly_ft4_buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            g_jobstts_gfx_draw_move_buffer, g_jobstts_gfx_draw_area_buffer);
        jobstts_menu_load_images_and_clear_selection();
        jobstts_unit_init_job_data(unit);
        menu_clear_selection_records();
        g_jobstts_initialized = 1;
    }
    jobstts_gfx_swap_context_and_clear_otag(0, -1);
    result = g_menu_gfx_context->otag;
    g_jobstts_previous_event_mode = g_menu_input_lock_mode;
    jobstts_input_update_with_message_state();
    if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
        menu_text_start_help_thread(3);
    }
    if (g_jobstts_previous_event_mode != 0 && g_menu_input_lock_mode == 0) {
        text_restore_section_pointers();
    }
    menu_set_palette_mode(g_menu_input_lock_mode);
    if (g_jobstts_active_page == MENU_JOB_LIST_STEP_JOBS) {
        g_jobstts_active_page = jobstts_menu_update_job_list();
        if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
            result = 0;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_jobstts_initialized = 0;
        }
    } else if (g_jobstts_active_page == MENU_JOB_LIST_STEP_ABILITIES) {
        g_jobstts_active_page = jobstts_ability_update_list_menu();
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

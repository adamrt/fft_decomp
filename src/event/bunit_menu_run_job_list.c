#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/main_sound.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 bunit_menu_run_job_list(void) {
    s16 out_a;
    s16 out_b;
    s32 mask;

    if (g_bunit_job_list_initialized == 0) {
        bunit_menu_set_event_speed(2);
        /* The caller still passes a third argument the definition ignores;
         * the target sets $a2 to 0 at this call. */
        ((s32 (*)(s32, s16*, s32))bunit_job_build_unit_job_list)((u8)g_bunit_unit_selected_index, g_bunit_job_ids, 0);
        g_bunit_menu_row_callbacks[0] = bunit_job_calculate_current_level;
        g_bunit_menu_row_callbacks[2] = (bunit_menu_row_callback_t)bunit_job_calculate_current_jp;
        g_bunit_menu_row_callbacks[5] = (bunit_menu_row_callback_t)bunit_job_calculate_next_level_jp_requirement;
        g_bunit_menu_row_callbacks[6] = (bunit_menu_row_callback_t)bunit_job_calculate_current_total_jp;
        g_bunit_menu_row_callbacks[13] = (bunit_menu_row_callback_t)bunit_job_calculate_current_mastered;
        bunit_menu_get_selection_record(9, &out_a, &out_b, g_bunit_job_ids);
        bunit_menu_init_scrollable_list((s32)g_bunit_job_ids, out_a, out_b, (s32)g_battle_text_section_pointers[6]);
        g_bunit_job_list_initialized = 1;
    }

    g_bunit_text_selection_id = g_bunit_job_ids[g_bunit_menu_list_selected_index] + 0x6000;
    mask = g_bunit_menu_input_active_mask;
    if (mask & PSX_PAD_CROSS) {
        bunit_menu_set_event_speed(0);
        g_bunit_sound_queued_effect_id = MAIN_SFX_CANCEL;
        g_bunit_job_list_initialized = 0;
        return -1;
    }
    if ((mask & PSX_PAD_CIRCLE) && g_bunit_menu_current_menu == 2) {
        g_bunit_job_selected_id = g_bunit_job_ids[g_bunit_menu_list_selected_index];
        g_bunit_job_selected_generic_index = bunit_job_get_generic_index(g_bunit_job_selected_id);
        bunit_job_calculate_current_level(g_bunit_menu_list_selected_index);
        bunit_job_calculate_current_jp(g_bunit_menu_list_selected_index);
        bunit_job_calculate_current_total_jp(g_bunit_menu_list_selected_index);
        bunit_job_calculate_next_level_jp_requirement(g_bunit_menu_list_selected_index);
        bunit_menu_set_selection_record(
            9, g_bunit_menu_list_selected_index, g_bunit_menu_scroll_base_index, (u16*)g_bunit_job_ids);
        g_bunit_job_list_initialized = 0;
        g_bunit_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        return 1;
    }
    bunit_menu_dispatch_with_override((s32)&g_bunit_job_list_menu_script, g_bunit_menu_input_active_mask, g_event_mode);
    return 0;
}

#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"

/* Push the proposition participant detail level (type 0x28).
 *
 * Copies the 0x40x0x100 VRAM block at (0x180, 0) to (0x200, 0x100), appends
 * the two frame windows, draws the selected participant and restarts the
 * three core background threads. Each window has its own pos/window pointer:
 * a single-set pointer keeps its address computation beside its first use.
 */
void wldcore_menu_push_participant_detail_level(s32 proposition, s32 participant) {
    RECT rect;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    point32_t* pos1;
    point32_t* pos2;
    s32 index;
    s32 depth;

    rect.x = 0x180;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x200, 0x100);
    DrawSync(0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.proposition = proposition;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.participant = participant;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.left_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x74;
    pos1->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].participant.right_window = index;
    g_wldcore_window_records[index].priority = 8;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x60;
    pos2->y = -0x69;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    wldcore_unit_build_status_panel_data(g_main_active_propositions[proposition].participant_indices[participant]);

    g_wldcore_thread8_params.x = 0;
    g_wldcore_thread8_params.y = 0;
    g_wldcore_threadc_params.y = 0;
    g_wldcore_thread9_params.y = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, world_menu_unit_status_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_wldcore_thread8_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_wldcore_threadc_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_wldcore_thread9_params, 0, 0);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].participant._unknown_08 = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}

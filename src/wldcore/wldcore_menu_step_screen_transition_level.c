#include "fft/wldcore.h"
#include "psx/etc.h"
#include "psx/gpu.h"

void wldcore_menu_step_screen_transition_level(wldcore_menu_screen_transition_level_t* level) {
    RECT rect;

    if (level->delay == 0) {
        if (level->phase == 0) {
            DrawSync(0);
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
            StoreImage(&rect, (u32*)g_wldcore_scratch_buffer);
            DrawSync(0);
            VSync(0);
            world_thread_set_parameters(4, 0, 0, 1);
            g_wldcore_loaded_background_set = -1;
            level->phase++;
            return;
        }
        if (world_thread_is_running(4) != 0) {
            return;
        }
        g_wldcore_menu_stack_depth--;
        switch (level->screen) {
        case 0x1C:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
            wldcore_menu_push_brave_story_level();
            break;
        case 0x1D:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
            wldcore_menu_push_story_event_text_level(g_main_replay_story_event_index);
            break;
        case 0x2B:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB8DD, 0);
            wldcore_list_open_tutorial_categories(level->param_a, level->param_b);
            break;
        case 0x2C:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB8DD, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~0x10;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~0x10;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~0x10;
            wldcore_list_open_formation_tutorials();
            break;
        case 0x2D:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB8DD, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~0x10;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~0x10;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~0x10;
            wldcore_list_open_tutorial_mask_1_entries();
            break;
        case 0x1B:
            world_thread_start(0xE, world_text_message_box_thread);
            world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
            wldcore_menu_push_treasure_detail_level(level->param_a);
            break;
        }
    } else {
        level->delay--;
    }
}

#include "fft/world.h"
#include "psx/types.h"

/* Thread body: pauses the four worker threads after this one, yields until
 * g_world_menu_transition_finished is raised (keeping the g_world_menu_transition_timeout timeout 100 ahead of
 * g_world_menu_transition_duration), then resets the state, waits for threads 13..10, points
 * g_world_text_section_pointers[31] at g_world_menu_at_list_text, resumes the worker threads and stops itself. */
void world_menu_transition_thread(void) {
    g_world_menu_transition_active = 1;
    g_world_menu_overlay_state = 2;
    world_thread_suspend(g_world_thread_current_id + 1);
    world_thread_suspend(g_world_thread_current_id + 2);
    world_thread_suspend(g_world_thread_current_id + 3);
    world_thread_suspend(g_world_thread_current_id + 4);
    while (g_world_menu_transition_timeout = g_world_menu_transition_duration + 100,
        g_world_menu_transition_finished == 0) {
        world_thread_yield();
    }
    g_world_menu_transition_finished = 0;
    g_world_menu_transition_timeout = 0;
    world_unit_start_view_thread(0, 0xFF, 0xFF);
    world_thread_wait_until_inactive(13);
    world_thread_wait_until_inactive(12);
    world_thread_wait_until_inactive(11);
    world_thread_wait_until_inactive(10);
    g_world_text_section_pointers[31] = g_world_menu_at_list_text;
    world_thread_resume(g_world_thread_current_id + 1);
    world_thread_resume(g_world_thread_current_id + 2);
    world_thread_resume(g_world_thread_current_id + 3);
    world_thread_resume(g_world_thread_current_id + 4);
    g_world_menu_transition_active = 0;
    g_world_menu_overlay_state = 0;
    world_thread_exit_current();
}

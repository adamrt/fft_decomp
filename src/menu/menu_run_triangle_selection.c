#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Wait for the camera and menu transition, then run Triangle-menu selection. */
void menu_run_triangle_selection(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    while (camera_is_active() != 0 || g_menu_transition_state != 0) {
        thread_yield();
    }
    script_pulse_tutorial_wait_value(0xFE);
    g_menu_hide_numeric_values = 0;
    menu_run_icon_selection_loop();
}

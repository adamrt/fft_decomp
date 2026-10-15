#include "fft/event_attack.h"
#include "fft/thread.h"

/* Enter the deployment screen's tiles-only view: hide the menu, reset the
 * cursor and grid slide, prepare the valid-tile primitives and start the
 * deployment render thread. */
void attack_deploy_init_valid_tiles(void) {
    g_attack_deploy_tiles_only_mode = 1;
    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_HIDDEN;
    g_attack_deploy_active_cursor = 0;
    g_attack_deploy_grid_slide_x = 0;
    g_attack_deploy_grid_slide_y = 0;
    g_attack_deploy_arrow_position_mode = 0;
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    thread_start(ATTACK_THREAD_SLOT_DEPLOY_RENDER, attack_deploy_run_render_thread);
    thread_set_parameters(ATTACK_THREAD_SLOT_DEPLOY_RENDER, 0, 0, 0);
}

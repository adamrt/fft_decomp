#include "fft/attack.h"
#include "psx/types.h"

void attack_deploy_init_valid_tiles(void) {
    g_attack_deploy_tiles_only_mode = 1;
    g_attack_deploy_menu_state = 5;
    g_attack_deploy_active_cursor = 0;
    g_attack_deploy_grid_slide_x = 0;
    g_attack_deploy_grid_slide_y = 0;
    g_attack_deploy_arrow_position_mode = 0;
    attack_out_prepare_valid_deployment_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    battle_thread_start(15, attack_deploy_run_render_thread);
    battle_thread_set_parameters(15, 0, 0, 0);
}

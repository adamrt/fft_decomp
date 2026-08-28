#include "fft/world.h"
#include "psx/types.h"

void world_shop_run_fur_equip_purchase_step(void) {
    world_shop_run_equip_candidate_step();
    if (g_world_shop_menu_step == 1)
        g_world_shop_menu_step = 15;
}

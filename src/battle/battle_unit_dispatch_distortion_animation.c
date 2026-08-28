#include "fft/battle.h"

typedef void (*battle_distortion_animation_handler_t)(battle_unit_misc_data_t* unit);

extern battle_distortion_animation_handler_t g_battle_unit_distortion_animation_handlers[];

void battle_unit_dispatch_distortion_animation(battle_unit_misc_data_t* unit) {
    u32 distortion_id = unit->distortion_animation_id;

    if (distortion_id != 0) {
        if (distortion_id < 0x12) {
            g_battle_unit_distortion_animation_handlers[distortion_id](unit);
        } else {
            main_system_handle_animation_exception(0x17);
        }
    }
}

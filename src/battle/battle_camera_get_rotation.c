#include "fft/battle_camera.h"

u16* battle_camera_get_rotation(void) {
    return (u16*)&g_battle_camera_render_state;
}

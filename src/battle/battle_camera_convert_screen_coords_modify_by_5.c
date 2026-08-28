#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_convert_screen_coords_modify_by_5(const s16* screen, s16* destination) {
    battle_camera_convert_screen_coords_by_rotation((const u16*)screen, destination, 5);
}

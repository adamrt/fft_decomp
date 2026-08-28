#include "fft/battle.h"

void battle_camera_set_zoom(const VECTOR* zoom) {
    g_battle_camera_zoom.vx = zoom->vx;
    g_battle_camera_zoom.vy = zoom->vy;
    g_battle_camera_zoom.vz = zoom->vz;
}

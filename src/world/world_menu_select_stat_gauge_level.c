#include "fft/world.h"
#include "psx/types.h"

void world_menu_select_stat_gauge_level(s32 x, s32 y, s32 otag_index, s32 value) {
    s32 level;
    world_oriented_sprite_t* sprite;

    if (value < 0x50) {
        level = 0;
        if (value >= 0x3C) {
            level = 1;
            if (value >= 0x42) {
                level = 2;
                if (value >= 0x48) {
                    level = 4;
                    if (value < 0x4C) {
                        level = 3;
                    }
                }
            }
        }
    } else {
        level = 0;
        if (value >= 0x5E) {
            level = 1;
            if (value >= 0x60) {
                level = 2;
                if (value >= 0x62) {
                    level = 4;
                    if (value < 0x64) {
                        level = 3;
                    }
                }
            }
        }
    }
    sprite = &g_world_job_wheel_mastered_icon_sprite;
    sprite->x = x + 7;
    g_world_job_wheel_mastered_icon_sprite.y = y - 0xC;
    g_world_job_wheel_mastered_icon_sprite.u = level * 10 + 0x24;
    world_gfx_enqueue_oriented_textured_quad((const world_oriented_quad_t*)sprite, 0, 0, 0, otag_index);
}

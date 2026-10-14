#include "fft/world.h"
#include "psx/types.h"

/*
 * Draws the mastered-job star over a job-wheel sprite at (x, y).
 *
 * `value` is the wheel's animation counter. The star twinkles twice per
 * cycle: it runs through frames 1..4 over counts 0x3C..0x4F, and again from
 * 0x5E (frame 4 from 0x64 on); otherwise it shows frame 0. Frames are 10
 * texels apart from u 0x24.
 */
void world_menu_select_stat_gauge_level(s32 x, s32 y, s32 otag_index, s32 value) {
    s32 level;
    oriented_quad_t* sprite;

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
    sprite->position.x = x + 7;
    g_world_job_wheel_mastered_icon_sprite.position.y = y - 0xC;
    g_world_job_wheel_mastered_icon_sprite.source.u = level * 10 + 0x24;
    menu_gfx_enqueue_oriented_textured_quad(sprite, 0, TEXTURE_UV_FLIP_NONE, 0, otag_index);
}

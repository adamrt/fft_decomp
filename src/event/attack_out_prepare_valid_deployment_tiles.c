#include "fft/event_attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Expands the squad's 5-by-5 deployment bitmap into g_attack_deploy_valid_tiles
 * for its orientation (skipped while g_attack_deploy_tiles_only_mode is set), then lays out one
 * valid/invalid tile sprite per cell.
 *
 * The y0 assignment is written out in both arms: the target's shared
 * subtract/add tail comes from cross-jumping those identical arm tails. */
void attack_out_prepare_valid_deployment_tiles(SPRT* sprites) {
    s32 x;
    s32 y;
    s32 bit;
    s32 orientation;
    s32 index;
    s32 lift;

    bit = 1;
    if (g_attack_deploy_tiles_only_mode == 0) {
        orientation = g_attack_deploy_current_squad_data->orientation_flags & ATTACK_DEPLOYMENT_ORIENTATION_MASK;
        if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_NORMAL) {
            for (y = 0; y < 5; y++) {
                for (x = 0; x < 5; x++) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_90) {
            for (x = 4; x >= 0; x--) {
                for (y = 0; y < 5; y++) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_180) {
            for (y = 4; y >= 0; y--) {
                for (x = 4; x >= 0; x--) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_270) {
            for (x = 0; x < 5; x++) {
                for (y = 4; y >= 0; y--) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        }
    }

    index = 0;
    for (y = 0; y < 5; y++) {
        for (x = 0; x < 5; x++) {
            if (g_attack_deploy_valid_tiles[y][x] != 0) {
                lift = -4;
                sprites[index].u0 = 0x28;
                sprites[index].v0 = 0xc0;
            } else {
                lift = 0;
                sprites[index].u0 = 0;
                sprites[index].v0 = 0xc0;
            }
            sprites[index].x0 = y * 20 + 0x100 - x * 20;
            if (g_attack_deploy_tiles_only_mode != 0) {
                sprites[index].y0 = 0x88 - x * 10 - y * 10 + lift;
            } else {
                sprites[index].y0 = 0xc4 - x * 10 - y * 10 + lift;
            }
            sprites[index].w = 0x28;
            sprites[index].h = 0x1c;
            sprites[index].x0 -= 0x14;
            index++;
        }
    }
}

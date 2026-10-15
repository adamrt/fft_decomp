#include "fft/world.h"
#include "psx/types.h"

/* Draw the formation units and the backdrop or egg decoration for each
 * visible slot. */
void world_formation_draw_unit_slots(point16_t* positions, s32 shade) {
    rgb24_t sprite_rgb;
    rgb24_t backdrop_rgb;
    s32 i;
    s32 ot_index;
    s32 palette;
    world_unit_t* unit;
    s32 brightness;
    s8 rgb_value;

    backdrop_rgb.b = shade;
    backdrop_rgb.g = shade;
    backdrop_rgb.r = shade;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        if (positions[i].x < -0x1E || positions[i].y < -0x1E || positions[i].x >= 0x101 || positions[i].y >= 0xF1) {
            continue;
        }
        if (i != g_unit_selected_index) {
            if (g_world_unit_pointers[i]->proposition_status & WORLD_FORMATION_UNIT_PROPOSITION_LEAVING) {
                palette = MENU_UNIT_READOUT_LEAVING;
            } else {
                if (g_world_unit_pointers[i]->gender_flags & UNIT_FLAG_EGG) {
                    palette = MENU_UNIT_READOUT_EGG;
                } else {
                    if (g_world_unit_pointers[i]->proposition_status & WORLD_FORMATION_UNIT_PROPOSITION_MISSING) {
                        palette = MENU_UNIT_READOUT_MISSING;
                    } else {
                        palette = g_world_formation_stat_display_mode;
                    }
                }
            }
            world_formation_draw_graphics_below_sprite(
                i, palette, positions[i].x, positions[i].y, &backdrop_rgb, 0, 0, 9);
            ot_index = 8;
        } else {
            ot_index = 10;
        }
        unit = g_world_unit_pointers[i];
        if (unit->gender_flags & UNIT_FLAG_EGG) {
            world_formation_draw_egg_sprite(&positions[i], unit->egg_color, 0, unit->birthday & 0xF, ot_index);
        } else {
            brightness = world_formation_cursor_distance_falloff(positions[i].x, positions[i].y, 0xC8, 0x50);
            if (brightness > 0x80) {
                brightness = 0x80;
            }
            rgb_value = brightness;
            sprite_rgb.b = rgb_value;
            sprite_rgb.g = rgb_value;
            sprite_rgb.r = rgb_value;
            world_formation_draw_unit_sprite(i, &positions[i], &sprite_rgb, ot_index);
        }
    }
}

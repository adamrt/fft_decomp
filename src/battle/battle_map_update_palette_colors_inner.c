#include "fft/battle.h"
#include "psx/types.h"

/* Update one 16-color map palette row immediately or stage an animation.
 *
 * Mode 0 decodes the PlayStation ABBBBBGGGGGRRRRR words into the runtime
 * component bank and rebuilds the upload copy while preserving each stored
 * alpha bit. Other modes store biased component deltas and arm the row's
 * animation state. */
void battle_map_update_palette_colors_inner(s32 mode, s32 palette_group, s32 palette_index, const u16* colors) {
    s32 index;
    s16 green_for_packing;
    u32 blue_delta;
    u16 red_for_packing;

    index = 0;
    do {
        s32 color_index;
        u16 color;
        map_palette_color_components_t* component;
        u8 green;
        u32 green_value;
        u32 blue_value;
        u8 red;
        u32 red_value;
        u8 blue;

        color_index = palette_index * 16 + index;
        component = &g_battle_map_palette_state.banks[palette_group].components[color_index];
        color = *colors;
        red_value = color & 0x1f;
        red_for_packing = red_value;
        red = red_for_packing;
        green_value = (color & 0x3e0) >> 5;
        green_for_packing = green_value;
        green = green_for_packing;
        blue_value = (color & 0x7c00) >> 10;
        blue = blue_value;

        if (mode != 0) {
            component->red_delta_biased = red - component->red_5bit + 0x1f;
            blue_delta = blue - component->blue_5bit + 0x1f;
            component->green_delta_biased = green - component->green_5bit + 0x1f;
            component->blue_delta_biased = blue_delta;
        } else {
            component->green_5bit = green;
            component->red_5bit = red;
            component->blue_5bit = blue;
            g_battle_map_palette_modified_colors[palette_group][color_index]
                = red_for_packing + (green_for_packing << 5) + (blue_value << 10) + (component->alpha_bit << 15);
        }

        index++;
        colors++;
    } while (index < 16);

    if (mode != 0) {
        map_palette_animation_state_t* animation;

        animation = &g_battle_map_palette_state.banks[palette_group].animations[palette_index];
        animation->active = 1;
        animation->blend_step = 0;
        animation->delay_counter = 0;
        animation->mode = mode;
    } else {
        g_battle_map_palette_state.banks[palette_group].animations[palette_index].active = 0;
    }
}

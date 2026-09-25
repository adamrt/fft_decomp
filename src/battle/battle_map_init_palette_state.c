#include "fft/battle.h"
#include "psx/types.h"

void battle_map_init_palette_state(void) {
    s32 group;
    s32 enabled;
    s32 animation_index;

    g_battle_map_palette_state.upload_pending = 0;
    g_battle_map_palette_state.move_pending = 0;

    enabled = 1;
    group = 0;
    do {
        g_battle_map_palette_state.banks[group].updates_disabled = 0;
        g_battle_map_palette_state.banks[group].enabled = enabled;

        animation_index = MAP_PALETTE_ROW_COUNT - 1;
        do {
            g_battle_map_palette_state.banks[group].animations[animation_index].active = 0;
            animation_index--;
        } while (animation_index >= 0);

        g_battle_map_palette_state.banks[group].components[0].red_delta_biased = 0;
        group++;
    } while (group < 14);

    g_map_background_gradient_transition.active = 0;
    g_map_background_gradient_transition._unknown_04[0] = 0;
    g_battle_map_ambient_light_transition.active = 0;
    g_battle_map_ambient_light_transition._unknown_04[0] = 0;
    g_battle_map_palette_state.banks[2].enabled = 0;
    g_battle_map_palette_state.banks[3].enabled = 0;
    g_battle_map_palette_state.banks[4].enabled = 0;
    g_battle_map_palette_state.banks[7].enabled = 0;
    battle_map_reset_texture_animations();
}

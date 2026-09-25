#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/types.h"

/* Per-frame WORLD sound effect dispatch: stops the previous looping sound
 * when a stop id (0x73/0x2d) is queued, remembers ids 2/3 as the held loop,
 * and plays whichever id remains. */
void world_sound_dispatch_effect(void) {
    s32 sound_id;

    if (g_world_sound_release_held_loop != 0) {
        g_world_menu_sound_effect_id = 0;
    }
    if (g_world_sound_effect_id_to_play == MAIN_SFX_TEXT_GLYPH
        || g_world_sound_effect_id_to_play == MAIN_SFX_TEXT_PAGE) {
        main_sound_play_sfx_find_channel(g_world_sound_effect_id_to_play);
        g_world_sound_effect_id_to_play = 0;
    } else if (g_world_menu_sound_effect_id == MAIN_SFX_CARD_ERROR) {
        main_sound_play_sfx_find_channel(MAIN_SFX_CARD_ERROR);
        g_world_menu_sound_effect_id = 0;
    }
    if ((u32)(g_world_sound_effect_id_to_play - 2) < 2) {
        g_world_menu_sound_effect_id = g_world_sound_effect_id_to_play;
    }
    if (g_world_sound_effect_id_to_play < 0 || g_world_menu_sound_muted != 0) {
        g_world_sound_effect_id_to_play = 0;
    }
    sound_id = g_world_sound_effect_id_to_play;
    if (sound_id == 0) {
        sound_id = g_world_menu_sound_effect_id;
    }
    g_world_menu_sound_effect_id = 0;
    g_world_sound_effect_id_to_play = 0;
    if (sound_id != 0) {
        main_sound_play_sfx(sound_id);
    }
}

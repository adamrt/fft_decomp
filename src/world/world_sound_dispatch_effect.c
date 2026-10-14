#include "fft/sound.h"
#include "fft/world.h"
#include "psx/types.h"

/* Per-frame WORLD sound effect dispatch. A queued text glyph or page sound
 * plays at once on a free channel pair, as does an error message sound in
 * the menu queue. Then the id in g_sound_pending_sfx_id (dropped
 * when negative or while the menu sounds are muted), or failing that the one
 * in g_world_menu_sound_effect_id, plays on the main_sound_play_sfx channels
 * and both queues are cleared. Cancel and Cursor Move are copied to the menu
 * queue before the mute test, so they still play while muted. */
void world_sound_dispatch_effect(void) {
    s32 sound_id;

    if (g_world_sound_release_held_loop != 0) {
        g_world_menu_sound_effect_id = MAIN_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == MAIN_SFX_TEXT_GLYPH || g_sound_pending_sfx_id == MAIN_SFX_TEXT_PAGE) {
        main_sound_play_sfx_find_channel(g_sound_pending_sfx_id);
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    } else if (g_world_menu_sound_effect_id == MAIN_SFX_ERROR_MESSAGE) {
        main_sound_play_sfx_find_channel(MAIN_SFX_ERROR_MESSAGE);
        g_world_menu_sound_effect_id = MAIN_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == MAIN_SFX_CANCEL || g_sound_pending_sfx_id == MAIN_SFX_CURSOR_MOVE) {
        g_world_menu_sound_effect_id = g_sound_pending_sfx_id;
    }
    if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    }
    sound_id = g_sound_pending_sfx_id;
    if (sound_id == MAIN_SFX_NONE) {
        sound_id = g_world_menu_sound_effect_id;
    }
    g_world_menu_sound_effect_id = MAIN_SFX_NONE;
    g_sound_pending_sfx_id = MAIN_SFX_NONE;
    if (sound_id != MAIN_SFX_NONE) {
        main_sound_play_sfx(sound_id);
    }
}

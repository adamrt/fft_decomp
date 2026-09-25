#include "fft/main.h"
#include "psx/types.h"

/* The shared body in main_sound_key_flush.h. The generated linker script only
 * binds names that appear in this file, so the ones that body uses are listed
 * here: g_main_sound_active_music_list, g_main_mask_exclusion, g_main_sound_music_key_off_voices,
 * g_main_sound_sfx_key_off_voices, SpuSetVoiceRRAttr and SpuSetKey. */
void main_sound_flush_key_off(void) {
    main_sound_flush_key_off_inline();
}

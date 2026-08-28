#include "fft/main_sound_key_flush.h"
#include "psx/types.h"

/* The shared body in main_sound_key_flush.h. The generated linker script only
 * binds names that appear in this file, so the ones that body uses are listed
 * here: g_main_sound_active_music_list, g_main_mask_exclusion, g_main_sound_sfx_key_off_voices and
 * SpuSetKey. */
void main_sound_flush_key_on(void) {
    main_sound_flush_key_on_inline();
}

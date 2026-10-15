#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Replace the effect sounds: unregister the sound resource of the previous effect
 * file, then register this file's sound section and take its id as the base for
 * the effect's sound ids. A file without a sound section clears both globals. */
void effect_load_sound_section(s16 index) {
    effect_file_header_t* base;
    s32 offset;

    if (g_effect_sound_section_ptr != 0) {
        suzuki_sound_remove_resource(g_effect_sound_section_ptr);
    }

    base = g_effect_data_ptrs[index];
    offset = base->sound_section;
    if (offset != 0) {
        g_effect_sound_section_ptr = (suzuki_sound_resource_t*)((u8*)base + offset);
        suzuki_sound_append_resource(g_effect_sound_section_ptr);
        g_effect_sound_data_base = SUZUKI_SOUND_ID(g_effect_sound_section_ptr->id, 0);
    } else {
        g_effect_sound_section_ptr = 0;
        g_effect_sound_data_base = 0;
    }
}

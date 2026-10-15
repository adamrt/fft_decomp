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

/* Start loading the effect file of g_battle_ability_effect_id; an effect whose
 * file has no size is replaced by the Cure effect. Nothing in the game calls it:
 * effect_init_data does the same in its first state. */
void effect_load_data(void) {
    s32 lba;
    s32 byte_length;

    effect_load_lba_and_size(g_battle_ability_effect_id, &lba, &byte_length);
    if (byte_length == 0) {
        g_battle_ability_effect_id = EFFECT_FILE_CURE;
        effect_load_lba_and_size(EFFECT_FILE_CURE, &lba, &byte_length);
    }
    main_file_call_build_header(lba, byte_length, 0);
}

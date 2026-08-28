#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_load_data(void) {
    s32 lba;
    s32 byte_length;

    battle_effect_load_lba_and_size(g_ability_effect_id, &lba, &byte_length);
    if (byte_length == 0) {
        g_ability_effect_id = 1;
        battle_effect_load_lba_and_size(1, &lba, &byte_length);
    }
    main_file_call_build_header(lba, byte_length, 0);
}

#include "fft/battle_effect.h"
#include "fft/main_sound.h"
#include "psx/types.h"

/*
 * Effect on-hit sound schedule: 0x00 halfword delay per entry, 0x12 byte
 * kind per entry (0/1 none, 2+ sound index + 2), 0x1c halfword entry count.
 * Sound ids are relative to g_battle_effect_sound_data_base, the effect sound
 * section's id set by battle_effect_load_sound_section.
 */
void battle_effect_update_on_hit_sound_timer(u8* schedule, s16* entry_index, s16* countdown) {
    s16 index = *entry_index;
    s32 effect_kind;
    s32 sound_index;

    if (index < *(s16*)(schedule + 0x1c)) {
        if (*countdown == 0) {
            *entry_index = index + 1;
            *countdown = *(u16*)((index << 1) + (s32)schedule);
            effect_kind = *(u8*)(schedule + index + 0x12);

            if (effect_kind >= 2) {
                sound_index = battle_effect_get_on_hit_sound_index(effect_kind - 2);

                if (sound_index != 0) {
                    SuzukiPlaySoundFindChannel(g_battle_effect_sound_data_base + sound_index);
                }
            }
        }
        *countdown -= 1;
    }
}

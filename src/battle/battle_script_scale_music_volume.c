#include "psx/types.h"

s32 battle_script_scale_music_volume(s32 volume) {
    if (volume == 0) {
        return 0;
    }
    if (volume >= 0x60) {
        volume = 0x7F;
    } else {
        volume = (volume * 0x7F) / 96;
    }
    return volume;
}

#include "fft/open.h"
#include "psx/types.h"

void open_sound_set_type_and_volume(s32 sound_type, s32 duration) {
    s32 volume;

    if (sound_type == 0) {
        volume = 0;
    } else {
        main_sound_put_type(sound_type);
        volume = 0x6400;
    }

    main_sound_set_cd_volume(volume, duration);
}

#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Shuts the driver down: closes its events, releases every voice with
 * release rate 6 and turns reverb off.
 *
 * The status word is tested as signed (`lh`); a u16 test loads with `lhu`. */
void main_sound_quit(void) {
    s32 voice;
    s16 status;

    status = g_main_sound_driver_flags;
    if (status != 0) {
        EnterCriticalSection();
        CloseEvent(g_main_root_counter_2_event);
        CloseEvent(g_main_sound_spu_event);
        g_main_sound_driver_flags = 0;
        ExitCriticalSection();
        for (voice = 0; voice < 24; voice++) {
            SpuSetVoiceRR(voice, 6);
        }
        SpuSetKey(0, 0xffffff);
        main_sound_set_reverb_mode(0, 0, 0, 0);
        D_80032A28 = 0;
    }
}

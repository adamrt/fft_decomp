#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Point the battle text sections at an overlay text file: 32 offsets
 * followed by the section data they are relative to. The event-message
 * section is left as it is. */
void attack_text_init_battle_pointers(attack_text_file_t* file) {
    s32* offsets;
    s32 i;
    u8* data;
    u8** table;

    offsets = file->section_offsets;
    i = 0;
    /* Section data follows the 32-entry offset header. */
    data = file->section_data;
    table = g_text_section_pointers;
    do {
        if (i != TEXT_SECTION_EVENT_MESSAGES) {
            *table = data + *offsets;
        }
        offsets++;
        i++;
        table++;
    } while (i < 32);
}

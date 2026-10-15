#include "fft/wldcore.h"
#include "psx/types.h"

/* Find an opcode in a sound-novel script section; return its whole instruction or zero. */
u32 wldcore_sound_novel_find_section_instruction(s32 section, s32 opcode) {
    u32* word;
    s32 i;

    word = &((u32*)&g_main_save_slot_buffer)[g_wldcore_active_saved_record.buffer->offsets[section] / sizeof(u32)];
    for (i = 0; i < 256; i++) {
        u32 value = *word;
        if (opcode == (value & 0xFF)) {
            return value;
        }
        if ((value & 0xFF) == 1) {
            return 0;
        }
        word++;
    }
    return 0;
}

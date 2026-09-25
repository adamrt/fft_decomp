#include "fft/event_card.h"
#include "psx/types.h"

void card_save_build_slot_description(s32 mode, u8* destination) {
    s32 index;
    volatile u8 stack_padding[4];
    const u8* source;

    if (mode == 1) {
        source = ((const u8* (*)(const u8*, s32, s32))card_text_skip_encoded_segments)(g_card_text_data, 14, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    } else if (mode == 0) {
        index = 0;
        while (g_card_save_buffer_pointer->name[index] != 0xfe) {
            *destination++ = g_card_save_buffer_pointer->name[index];
            index++;
        }

        *destination++ = 0xe7;
        *destination++ = 0x58;
        source = ((const u8* (*)(const u8*, s32, s32))card_text_skip_encoded_segments)(
            g_card_text_job_name, g_card_save_buffer_pointer->job_id, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xe7;
        *destination++ = 0xac;
        source = ((const u8* (*)(const u8*, s32, s32))card_text_skip_encoded_segments)(g_card_text_data, 13, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = g_card_save_buffer_pointer->level / 10;
        *destination++ = g_card_save_buffer_pointer->level % 10;
        *destination++ = 0xfe;
        source = ((const u8* (*)(const u8*, s32, s32))card_text_skip_encoded_segments)(
            g_card_text_data, g_card_save_buffer_pointer->month, 1);
        while (*source != 0xfe) {
            *destination++ = *source++;
        }

        *destination++ = 0xfa;
        *destination++ = g_card_save_buffer_pointer->day / 10;
        *destination++ = g_card_save_buffer_pointer->day % 10;
        *destination++ = 0xe7;
        *destination++ = 0x58;
        source = ((const u8* (*)(const u8*, s32, s32))card_text_skip_encoded_segments)(
            g_card_text_location_name, g_card_save_buffer_pointer->location, 1);
        while ((*destination++ = *source++) != 0xfe) { }
    }
    *destination = 0xfe;
}

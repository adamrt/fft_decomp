#include "fft/event_card.h"

enum {
    CARD_SAVE_HEADER_MAGIC_S = 'S',
    CARD_SAVE_HEADER_MAGIC_C = 'C',
    CARD_SAVE_HEADER_ICON_ONE_FRAME = 0x11,
    CARD_SAVE_HEADER_BLOCK_COUNT_ONE = 1,
};

/* Build the 256-byte PlayStation memory-card header: the title template with
 * the 1-based save slot number and the play time as full-width Shift-JIS
 * digits, then the icon selected by the slot. `level` is not read.
 */
void card_card_build_save_file_header(s32 slot, s32 level, card_save_header_t* header) {
    s32 value;
    s32 digit;
    s32 divisor;
    s32 pos;
    s32 unused;
    card_save_icon_t* records;
    card_save_icon_t* record;

    header->magic0 = CARD_SAVE_HEADER_MAGIC_S;
    header->magic1 = CARD_SAVE_HEADER_MAGIC_C;
    header->icon_flags = CARD_SAVE_HEADER_ICON_ONE_FRAME;
    header->block_count = CARD_SAVE_HEADER_BLOCK_COUNT_ONE;
    memset(header->reserved, 0, 0x1C);
    slot++;
    bcopy(g_card_save_title_template, header->title, 0x16);

    value = slot / 10;
    header->title[0x10] = 0x82;
    header->title[0x11] = value + 0x4F;
    value = slot % 10;
    header->title[0x12] = 0x82;
    header->title[0x13] = value + 0x4F;

    pos = 0x16;

    value = g_main_system_play_time_hours;
    if (value >= 100) {
        value = 99;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_main_system_play_time_minutes;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0x81;
    header->title[pos++] = 0x46;

    value = g_main_system_play_time_seconds;
    if (value >= 60) {
        value = 59;
    }
    divisor = 10;
    while (divisor > 0) {
        digit = value / divisor;
        value = value % divisor;
        header->title[pos++] = 0x82;
        header->title[pos++] = digit + 0x4F;
        divisor = divisor / 10;
    }

    header->title[pos++] = 0;
    header->title[pos++] = 0;

    records = g_card_save_icon_records;
    value = (slot - 1) % 15;
    record = records + value;
    bcopy(record->clut, header->icon_clut, 0x20);
    bcopy(record->image, header->icon_image, 0x80);
}

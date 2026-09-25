#include "fft/world.h"
#include "psx/types.h"

s32 world_text_adjust_value_for_event_code(s32 event_code, s32 value) {
    if (event_code == TEXT_FORMAT_RAMZA_NAME) {
        return TEXT_ID_UNIT_NAME_BASE;
    }
    if (event_code == TEXT_FORMAT_UNIT_NAME) {
        return value + TEXT_ID_UNIT_NAME_BASE;
    }
    if (event_code == TEXT_FORMAT_SECTION_9000) {
        return value + TEXT_ID_SECTION_9000_BASE;
    }
    if (event_code == TEXT_FORMAT_ITEM_NAME) {
        return value + TEXT_ID_ITEM_NAME_BASE;
    }
    if (event_code == TEXT_FORMAT_ABILITY_NAME) {
        return value + TEXT_ID_ABILITY_NAME_BASE;
    }
    if (event_code == TEXT_FORMAT_EXPLICIT_ID) {
        return value;
    }
    return -1;
}

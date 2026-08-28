#include "fft/wldcore.h"

void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record) {
    record->text_history_3 = -1;
    record->text_history_2 = -1;
    record->text_history_1 = -1;
    record->text_history_0 = -1;
    record->picture_id = -1;
    record->background_set = -1;
    record->sound_mode = 0;
    record->music_selection = 0;
    record->sound_id = 0;
    record->state_flags = 0;
    record->section = 0;
    record->counter = 0;
    record->field_18 = 0;
    record->text_id = 0;
    record->counter_delta = 0;
    record->brightness = 0x30;
}

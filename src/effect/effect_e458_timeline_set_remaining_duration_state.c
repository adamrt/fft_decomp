#include "fft/effect.h"

typedef struct effect_timeline_header {
    u16 _unused_00;
    u16 effect_duration;
} effect_timeline_header_t;

s32 effect_e458_timeline_set_remaining_duration_state(s16 record_index, s32 byte_offset) {
    effect_record_t* record = &g_effect_state_records[record_index];

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        battle_map_set_command_0x96_duration(
            0x96, g_effect_timeline_channel_base->effect_duration - record->timeline_frame);
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        break;
    case EFFECT_PHASE_DESTROY:
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}

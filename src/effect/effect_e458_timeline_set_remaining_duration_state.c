#include "fft/battle.h"
#include "fft/effect.h"

typedef struct effect_timeline_record_view {
    u8 padding[0x28];
    s16 effect_timer;
} effect_timeline_record_view_t;

typedef struct effect_timeline_header {
    u16 unknown_0;
    u16 effect_duration;
} effect_timeline_header_t;

s32 effect_e458_timeline_set_remaining_duration_state(s16 record_index, s32 byte_offset) {
    s32 record_offset;
    u8* table;
    u8* record;
    effect_state_t* state;
    record_offset = record_index * sizeof(effect_record_t);
    table = (u8*)g_effect_state_records;
    record = table + record_offset;
    state = (effect_state_t*)(record + byte_offset);

    switch (state->phase) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        battle_map_set_command_0x96_duration(0x96,
            g_effect_timeline_channel_base->effect_duration - ((effect_timeline_record_view_t*)record)->effect_timer);
        state->phase = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        break;
    case EFFECT_PHASE_DESTROY:
        state->phase = EFFECT_PHASE_IDLE;
        break;
    }
}

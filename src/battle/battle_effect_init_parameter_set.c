#include "fft/battle_effect.h"
#include "psx/types.h"

/* Point a sprite animation node at parameter set animation_index and restart
 * it: clear the sequence position and hold time, store frame_group doubled and
 * set kind bit 0 to mark the frame dirty. */
void battle_effect_init_parameter_set(effect_list_node_t* state, s32 frame_group, s32 animation_index) {
    u8* parameter_sets;
    u8* entry;
    s32 offset;
    u16 flags;

    parameter_sets = g_battle_effect_parameter_sets;
    entry = (u8*)(((animation_index << 16) >> 15) + (s32)parameter_sets);
    flags = state->kind | 1;
    offset = entry[0] + (entry[1] << 8);
    state->sequence_offset = 0;
    state->frame_group_index = (s8)((frame_group & 0xFF) * 2);
    state->frame_timer = 0;
    state->kind = flags;
    state->sequence_data = (s32)(s16)offset + (s32)parameter_sets;
}

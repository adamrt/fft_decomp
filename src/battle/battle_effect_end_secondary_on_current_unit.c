#include "fft/battle.h"
#include "fft/battle_effect.h"

/*
 * Each store addresses a field of g_battle_effect_secondary_data (0x54-byte
 * records) as field address + record byte offset: the target emits a
 * `lui at / addu / store %lo(field)` per store in this order, which record
 * indexing does not reproduce (GCC reschedules the stores).
 */
enum { SECONDARY_EFFECT_RECORD_SIZE = 0x54, SECONDARY_EFFECT_STATE_ENDING = 0x15 };

s32 battle_effect_end_secondary_on_current_unit(void) {
    s32 target_id = battle_effect_allocate_secondary_slot() & 0xFF;
    s32 record_offset = target_id * SECONDARY_EFFECT_RECORD_SIZE;
    u8 function_id;

    *(s16*)((u8*)&g_battle_effect_secondary_data[0].timer + record_offset) = 0;
    *(u16*)((u8*)&g_battle_effect_secondary_data[0].animation_id + record_offset) = SECONDARY_EFFECT_STATE_ENDING;
    function_id = g_battle_effect_ending_secondary_function_id;
    *(s32*)((u8*)&g_battle_effect_secondary_data[0].phase + record_offset) = BATTLE_SECONDARY_EFFECT_INITIALIZING;
    *((u8*)&g_battle_effect_secondary_data[0].function_id + record_offset) = function_id;
    return target_id;
}

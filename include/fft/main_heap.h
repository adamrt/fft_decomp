#ifndef FFT_MAIN_HEAP_H
#define FFT_MAIN_HEAP_H

#include "fft/battle_runtime.h"
#include "psx/types.h"

extern u8* g_main_heap_high_overlay_load_address;
extern u8* g_main_heap_low_overlay_load_address;
extern u8* g_main_heap_world_overlay_load_address;

/* ai */
void battle_ai_clear_words(s32* destination, s32 byte_count);

/* menu */
void battle_menu_free_memory(void* buffer);

/* heap */
void* main_heap_alloc(u32 size);
void* main_heap_alloc_smd(u32 size);
void main_heap_call_free(void* allocation);
void main_heap_clear_game_allocator_table(void);
s32 main_heap_free(void* allocation);
u8* main_heap_reserve_at(void* allocation, u32 size);
s32 main_heap_free_smd(void* allocation);

/* util */
void main_util_clear_byte_data(void* destination, s32 count);

/* SCUS byte loops at 0x8005e254 (copy) and 0x8005e644 (clear), not SDK
 * memcpy/memset. Copy takes source first;
 * both routines do nothing for a nonpositive signed byte count. */
void main_util_copy_byte_data(const void* source, void* destination, s32 count);

void* game_malloc(s32 byte_length);
void main_util_copy_action_data(const u8* source, u8* destination);

#endif

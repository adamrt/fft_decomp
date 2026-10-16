#ifndef MAIN_FILE_H
#define MAIN_FILE_H

/* Resident file loading, the heaps and overlay loading. */

#include "fft/main.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* file */
extern main_file_load_descriptor_t g_main_file_cd_state;
extern u32 g_main_file_still_loading;
/* File-header wrapper at 0x80011bd0 forwards main_file_build_header's result. */
s32 main_file_build_header_nnl(main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);
s32 main_file_call_build_header(s32 sector, u32 size, void* destination);
int main_file_is_still_loading(void);
void main_file_reset_cd_subsystems(void);
void main_file_handle_cd_read_callback(u8 event, u8* result);
void main_file_handle_cd_ready_callback(u8 event, u8* result);
int main_file_build_header(
    main_file_load_descriptor_t* state, int lba, int sector_count, void* destination, int loading_display_mode);
void* main_file_load_checked_to_heap(int sector, int size);
void* main_file_load_checked_to_address(int sector, int size, void* destination);
suzuki_smd_header_t* main_file_get_smd(int sector, u32 size);
void* main_file_load_to_address(int sector, u32 size, void* destination);
void main_file_poll_load(main_file_load_descriptor_t* state);
void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state);

/* overlay */
int main_overlay_exec_open_bin_main_loop(int mode);
void main_overlay_exec_open_bin_ending(void);
int main_overlay_open_world_and_wldcore(int load_world);
void main_overlay_open_world_bin(s32 mode);

/* heap */
extern u8 g_main_heap_game_allocator_table[MAIN_HEAP_GAME_PAGE_COUNT];
extern u8 g_main_heap_smd_allocator_table[MAIN_HEAP_SMD_PAGE_COUNT];
extern u8 g_main_heap_smd_base[];
extern u8* g_main_heap_high_overlay_load_address;
extern u8* g_main_heap_low_overlay_load_address;
extern u8* g_main_heap_world_overlay_load_address;
extern u8* g_main_heap_companion_overlay_load_address;
void* main_heap_alloc(u32 size);
void* main_heap_alloc_smd(u32 size);
void main_heap_call_free(void* allocation);
void main_heap_clear_game_allocator_table(void);
s32 main_heap_free(void* allocation);
u8* main_heap_reserve_at(u8* allocation, u32 size);
s32 main_heap_free_smd(void* allocation);
void main_heap_clear_smd_allocator_table(void);

#endif

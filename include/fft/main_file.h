#ifndef FFT_MAIN_FILE_H
#define FFT_MAIN_FILE_H

#include "psx/types.h"

/* One poll advances at most one phase; recoverable CD errors restart setup. */
typedef enum main_file_load_state {
    MAIN_FILE_LOAD_STATE_IDLE = 0,
    MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED = 1,
    MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE = 2,
    MAIN_FILE_LOAD_STATE_START_SEEK = 3,
    MAIN_FILE_LOAD_STATE_POLL_SEEK = 4,
    MAIN_FILE_LOAD_STATE_START_READ = 5,
    MAIN_FILE_LOAD_STATE_POLL_READ = 6,
} main_file_load_state_e;

/* Asynchronous CD read state advanced by main_file_poll_load. */
typedef struct main_file_load_descriptor {
    s32 unknown_00;
    s32 state; /* MAIN_FILE_LOAD_STATE_* */
    s32 error_count;
    s32 wait_frames;
    s32 sector_count;
    s32 lba;
    s32 sector_index; /* 0x18; sector being read by wldcore_file_poll_vram_image_stream */
    u8 position[4];
    u8* destination;
} main_file_load_descriptor_t;

typedef char main_file_load_state_size_must_be_36[sizeof(main_file_load_descriptor_t) == 36 ? 1 : -1];

extern main_file_load_descriptor_t g_main_file_cd_state;
extern u32 g_main_file_still_loading;

/* entd */
int main_entd_open_file(void);

/* file */
int main_file_build_header(
    main_file_load_descriptor_t* state, int lba, int sector_count, void* destination, int loading_display_mode);
void* main_file_get_bin_as_tim(int sector, int size);
void* main_file_load_checked_to_address(int sector, int size, void* destination);
void* main_file_get_smd(int sector, int size);
void* main_file_get_tim(s32 sector, s32 size);
void* main_file_load_to_address(int sector, u32 size, void* destination);
void main_file_load_data_from_disc(int sector, int sectors, void* destination, int suppress_loading_display);
void main_file_poll_load(main_file_load_descriptor_t* state);
void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state);
#endif

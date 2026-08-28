#ifndef FFT_MAIN_BOOT_H
#define FFT_MAIN_BOOT_H

#include "psx/types.h"

extern s32 g_main_boot_startup2_has_run;

/* boot */
void main_boot_run_startup(void);
void main_boot_reset_game_state(void);

/* card */
void main_card_init_events(void);

/* file */
void main_file_handle_cd_read_callback(void);
void main_file_handle_cd_ready_callback(void);
void main_file_reset_cdrom_cpu_ram(void);

/* item */
void main_item_init_order_tables(void);

/* save */
void main_save_init_data_tables(void);
void main_save_init_state_and_options(void);

/* status */
void main_status_init_check_data(void);

/* system */
void main_system_handle_draw_sync_callback(void);
void main_system_handle_vsync_callback(void);
#endif

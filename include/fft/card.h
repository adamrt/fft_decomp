#ifndef CARD_H
#define CARD_H

/* The memory card library compiled into the CARD overlay, OPEN and WORLD. */

#include "psx/types.h"

struct DIRENTRY;
struct card_save_buffer;

/* results */
/* The BIOS and hardware card pollers return the first event observed in the
 * IOE, ERROR, TIMOUT, NEW order installed by main_card_init_events. */
typedef enum card_io_result {
    CARD_IO_RESULT_PENDING = -1,
    CARD_IO_RESULT_COMPLETE = 0,
    CARD_IO_RESULT_ERROR = 1,
    CARD_IO_RESULT_TIMEOUT = 2,
    CARD_IO_RESULT_NEW_CARD = 3,
} card_io_result_e;

/* state */
extern u8 g_card_slot;
extern u8 g_card_info_pending;
extern s32 g_card_file_open_descriptor;

/* slot and events */
void card_io_set_selected_slot(s32 slot);
void card_io_consume_all_events(void);
s32 card_io_poll_bios_event_result(void);
s32 card_io_wait_for_bios_event(void);

/* card operations */
s32 card_io_get_info_with_retries_variant(s32 port, s32 retry_limit);
s32 card_io_poll_operation_result_variant(void);
s32 card_io_wait_operation_result(s32 retry_limit);
s32 card_io_open_or_create_selected_card(void);
s32 card_io_format_selected_slot(void);

/* files */
s32 card_file_enumerate_saves(struct DIRENTRY* entries, s32 all_files);
s32 card_file_count_free_blocks(const struct DIRENTRY* entries, s32 entry_count);
s32 card_file_open_selected_with_retries(const char* filename, s32 mode);
s32 card_file_close_with_retries(s32 descriptor);
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 card_file_write_buffer(const char* filename, const struct card_save_buffer* source, u32 size, s32 create_file);
s32 card_file_delete_selected_with_retries(const char* filename);
s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size);
s32 card_file_write_with_retries(s32 descriptor, const void* source, s32 size);

#endif

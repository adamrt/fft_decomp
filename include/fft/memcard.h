#ifndef MEMCARD_H
#define MEMCARD_H

/* The memory card library compiled into the CARD overlay, OPEN and WORLD, and the card BIOS events MAIN opens. */

#include "psx/types.h"

/* card events */
/* Memory-card BIOS event handles opened by memcard_init_events; the CARD,
 * OPEN and WORLD pollers test them by these indices. */
enum {
    MEMCARD_EVENT_BIOS_DONE,
    MEMCARD_EVENT_BIOS_ERROR,
    MEMCARD_EVENT_BIOS_TIMEOUT,
    MEMCARD_EVENT_BIOS_NEW_CARD,
    MEMCARD_EVENT_HARDWARE_DONE,
    MEMCARD_EVENT_HARDWARE_ERROR,
    MEMCARD_EVENT_HARDWARE_TIMEOUT,
    MEMCARD_EVENT_HARDWARE_NEW_CARD,
    MEMCARD_EVENT_COUNT
};

/* The BIOS and hardware card pollers return the first event observed in the
 * IOE, ERROR, TIMOUT, NEW order installed by memcard_init_events. */
typedef enum memcard_result {
    MEMCARD_RESULT_PENDING = -1,
    MEMCARD_RESULT_COMPLETE = 0,
    MEMCARD_RESULT_ERROR = 1,
    MEMCARD_RESULT_TIMEOUT = 2,
    MEMCARD_RESULT_NEW_CARD = 3,
} memcard_result_e;

extern s32 g_memcard_bios_events[MEMCARD_EVENT_COUNT];
extern u8 g_memcard_bios_events_opened;

void memcard_init_events(void);
void memcard_consume_all_events(void);
s32 memcard_poll_bios_event_result(void);
s32 memcard_wait_for_bios_event(void);

/* card operations */
extern u8 g_memcard_slot;
extern u8 g_memcard_info_pending;

void memcard_set_selected_slot(s32 slot);
s32 memcard_wait_operation_result(s32 retry_limit);
s32 memcard_open_or_create_selected_card(void);
s32 memcard_format_selected_slot(void);

/* OPEN builds its own bodies of these two. */
s32 memcard_get_info_with_retries(s32 port, s32 retry_limit);
s32 memcard_poll_operation_result(void);

/* files */
struct DIRENTRY;
struct save_image;

s32 memcard_file_enumerate_saves(struct DIRENTRY* entries, s32 all_files);
s32 memcard_file_count_free_blocks(const struct DIRENTRY* entries, s32 entry_count);
s32 memcard_file_open_selected_with_retries(const char* filename, s32 mode);
s32 memcard_file_close_with_retries(s32 descriptor);
s32 memcard_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 memcard_file_write_buffer(const char* filename, const struct save_image* source, u32 size, s32 create_file);
s32 memcard_file_delete_selected_with_retries(const char* filename);
s32 memcard_file_read_with_retries(s32 descriptor, void* destination, s32 size);
s32 memcard_file_write_with_retries(s32 descriptor, const void* source, s32 size);

#endif

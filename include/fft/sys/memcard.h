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
    MEMCARD_RESULT_NEW_CARD = 3
} memcard_result_e;
typedef char memcard_result_size_must_be_4[(sizeof(memcard_result_e) == 4) ? 1 : -1];

extern s32 g_memcard_bios_events[MEMCARD_EVENT_COUNT];

void memcard_init_events(void);
void memcard_clear_events(void);
memcard_result_e memcard_poll_bios_event(void);
memcard_result_e memcard_wait_bios_event(void);

/* card operations */
extern u8 g_memcard_slot;
extern u8 g_memcard_info_pending;

void memcard_set_slot(s32 slot);
memcard_result_e memcard_wait_operation(s32 retry_limit);
memcard_result_e memcard_open_slot(void);
s32 memcard_format_slot(void);

/* OPEN builds its own bodies of these two. */
memcard_result_e memcard_info_retry(s32 port, s32 retry_limit);
memcard_result_e memcard_poll_operation(void);

/* Empty. Called before a soft reset and when the memory card screen closes. */
void memcard_close_stub(void);

/* files */
struct DIRENTRY;

s32 memcard_enumerate_saves(struct DIRENTRY* entries, s32 all_files);
s32 memcard_count_free_blocks(const struct DIRENTRY* entries, s32 entry_count);
s32 memcard_file_open_retry(const char* filename, s32 mode);
s32 memcard_file_close_retry(s32 descriptor);
s32 memcard_file_seek_retry(s32 descriptor, s32 offset, s32 origin);
s32 memcard_file_delete_retry(const char* filename);
s32 memcard_file_read_retry(s32 descriptor, void* destination, s32 size);
s32 memcard_file_write_retry(s32 descriptor, const void* source, s32 size);

#endif

#ifndef CARD_H
#define CARD_H

/* Memory card I/O shared by the CARD overlay, OPEN and WORLD. */

#include "psx/types.h"

/* event waits */
s32 card_wait_for_bios_event_variant(void);
s32 card_wait_for_hardware_event_variant(void);

/* state */
extern u8 g_card_slot;

/* event waits */
s32 card_get_info_with_retries_variant(s32 port, s32 retry_limit);

/* state */
extern u8 g_card_info_pending;

/* unified */
void card_set_slot(s32 slot);
s32 card_poll_info_result_variant(void);

#endif

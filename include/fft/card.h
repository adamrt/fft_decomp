#ifndef CARD_H
#define CARD_H

/* Memory card I/O shared by the CARD overlay, OPEN and WORLD. */

#include "psx/types.h"

/* unified */
s32 card_wait_for_bios_event(void);
extern u8 g_card_slot;
s32 card_wait_for_hardware_event(void);

#endif

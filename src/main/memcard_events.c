/* Resident BIOS event setup shared by OPEN, WORLD and CARD. */

#include "fft/memcard.h"
#include "psx/kernel.h"
#include "psx/libapi.h"

extern u8 g_memcard_bios_events_opened;

/* MAIN keeps these BIOS event handles for the card pollers in OPEN, WORLD and CARD. */
s32 g_memcard_bios_events[MEMCARD_EVENT_COUNT] = { 0 };

/* Opens and enables the eight memory-card BIOS events (SwCARD then HwCARD,
 * each with IOE, ERROR, TIMOUT and NEW) once. */
void memcard_events_init(void) {
    s32 i;

    if (g_memcard_bios_events_opened != 0) {
        return;
    }
    i = 0;
    EnterCriticalSection();
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_DONE] = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_ERROR] = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_TIMEOUT] = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_BIOS_NEW_CARD] = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_DONE] = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_ERROR] = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_TIMEOUT] = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_NEW_CARD] = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    InitCARD(1);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();
    _card_auto(0);
    for (i = 0; i < MEMCARD_EVENT_COUNT; i++) {
        EnableEvent(g_memcard_bios_events[i]);
    }
    ExitCriticalSection();
    g_memcard_bios_events_opened = 1;
}

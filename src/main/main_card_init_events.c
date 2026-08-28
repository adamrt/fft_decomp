#include "fft/main_runtime.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Opens and enables the eight memory-card BIOS events (SwCARD then HwCARD,
 * each with IOE, ERROR, TIMOUT and NEW) once. */
void main_card_init_events(void) {
    s32 i;

    if (g_main_card_bios_events_opened != 0) {
        return;
    }
    i = 0;
    EnterCriticalSection();
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE] = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR] = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT] = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD] = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE] = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR] = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT] = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, 0);
    g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD] = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, 0);
    init_card_earlysafe(1);
    StartCARD_earlysafe();
    ChangeClearPad(0);
    _bu_init();
    _card_auto(0);
    for (i = 0; i < MAIN_CARD_EVENT_COUNT; i++) {
        EnableEvent(g_main_card_bios_events[i]);
    }
    ExitCriticalSection();
    g_main_card_bios_events_opened = 1;
}

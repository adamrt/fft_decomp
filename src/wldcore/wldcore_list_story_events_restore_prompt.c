#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Resume handler of the story events list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB848 again. */
void wldcore_list_story_events_restore_prompt(void) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

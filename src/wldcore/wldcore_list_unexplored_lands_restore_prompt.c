#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Resume handler of the unexplored lands list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB848 again. */
void wldcore_list_unexplored_lands_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
}

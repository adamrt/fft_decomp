#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

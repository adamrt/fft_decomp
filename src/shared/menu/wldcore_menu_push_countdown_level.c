#include "fft/thread.h"
#include "fft/wldcore.h"

/* Pushes a countdown menu level (type 0x37): the first message is 0xb8f9
 * when g_wldcore_proposition_report.reward_type is 1, otherwise 0xb904; a count of 1 from 0x8007a72c runs
 * four messages, any other count runs one message starting count + 2 further
 * on. Starts thread 14 if idle and shows the first message. */
void wldcore_menu_push_countdown_level(void) {
    s32 count;

    count = wldcore_get_completion_milestone_rank();
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_TREASURE_FOUND_FIRST;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_LAND_FOUND_FIRST;
    }
    if (count == 1) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 4;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 1;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current += 2 + count;
    }
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }
    wldcore_menu_show_next_sequence_message(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COUNTDOWN;
    g_wldcore_menu_stack_depth++;
}

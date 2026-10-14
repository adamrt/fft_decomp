#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

/* Event instruction 0x37 (unused by the scenario scripts): toggles bit 0 of
 * portrait_flipped of the menu window buffer whose message_id matches the
 * operand.
 *
 * record_index is never assigned: the target indexes with whatever $s0 holds
 * on entry and tests that one record six times. */
void script_toggle_message_portrait_flip(const u8* parameters) {
    s32 i;
    s32 record_index;
    s16 record_id;
    menu_window_buffer_t* record;

    record_id = script_read_s16(parameters);
    for (i = 0; i < 6; i++) {
        if (g_menu_window_buffers[record_index].message_id == record_id) {
            record = &g_menu_window_buffers[record_index];
            record->portrait_flipped = g_menu_window_buffers[record_index].portrait_flipped + 1;
            g_menu_window_buffers[record_index].portrait_flipped
                = g_menu_window_buffers[record_index].portrait_flipped & 1;
        }
    }
}

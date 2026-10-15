#include "fft/event_card.h"
#include "psx/types.h"

/* Bring the selected card to a readable state: acknowledge a newly inserted
 * card, then load its directory. Returns CARD_IO_RESULT_COMPLETE,
 * CARD_IO_RESULT_NEW_CARD (the load still reports a new, unformatted card)
 * or CARD_IO_RESULT_TIMEOUT for any failure. */
s32 card_io_open_or_create_selected_card(void) {
    s32 result = card_io_wait_operation_result(10);

    if (result == CARD_IO_RESULT_NEW_CARD) {
        result = card_io_create_new_with_retries(g_card_io_selected_slot << 4, 10);
        if (result != CARD_IO_RESULT_COMPLETE)
            return CARD_IO_RESULT_TIMEOUT;
    }
    if (result == CARD_IO_RESULT_COMPLETE)
        result = card_io_load_with_retries(g_card_io_selected_slot << 4, 30);
    if (result == CARD_IO_RESULT_NEW_CARD)
        return CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_COMPLETE)
        result = CARD_IO_RESULT_TIMEOUT;
    return result;
}

/* Card info with retries, info result polling, polling until stable and slot checks. */

#include "fft/memcard.h"
#include "fft/title.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Issue _card_info on `port` up to retry_limit times, waiting for its BIOS event
 * each time. Returns complete on the first success, otherwise the result of the
 * last attempt (error when _card_info itself refused). */
s32 memcard_get_info_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = memcard_wait_for_bios_event();
            if (result == MEMCARD_RESULT_COMPLETE)
                break;
        } else {
            result = MEMCARD_RESULT_ERROR;
        }
    }
    return result;
}

/* Issue _card_info on the selected slot once, then poll for its result; the
 * request is issued again when card status bit 0 is set with no event. A result
 * other than complete is checked again with two blocking attempts, and error is
 * reported as timeout. */
s32 memcard_poll_operation_result(void) {
    s32 status;
    s32 result;

    if (g_memcard_info_pending == 0) {
        _card_info(g_memcard_slot << 4);
        g_memcard_info_pending = 1;
    }

    status = _card_status(g_memcard_slot) & 1;
    result = memcard_poll_bios_event_result();
    if (status != 0 && result == MEMCARD_RESULT_PENDING) {
        g_memcard_info_pending = 0;
    }
    if (result >= MEMCARD_RESULT_COMPLETE) {
        g_memcard_info_pending = 0;
    }
    if (result > MEMCARD_RESULT_COMPLETE) {
        result = memcard_get_info_with_retries(g_memcard_slot << 4, 2);
    }
    if (result == MEMCARD_RESULT_ERROR) {
        result = MEMCARD_RESULT_TIMEOUT;
    }
    return result;
}

/* Probe the selected slot until three probes in a row complete, and return
 * complete. Any other settled result counts against retry_limit and is returned
 * when the limit is reached. A nonpositive limit leaves the result unset. */
static s32 memcard_poll_info_until_stable(s32 retry_limit) {
    s32 attempts = 0;
    s32 consecutive_successes = 0;
    s32 result;

    while (attempts < retry_limit) {
        do {
            result = memcard_poll_operation_result();
        } while (result == MEMCARD_RESULT_PENDING);

        if (result == MEMCARD_RESULT_COMPLETE) {
            consecutive_successes++;
            attempts--;
            if (consecutive_successes == 3) {
                break;
            }
        } else {
            consecutive_successes = 0;
        }
        attempts++;
    }
    return result;
}

/* 1 when a memory card answers in either slot, 0 when neither does. Slot 1 is
 * probed first, then slot 0, which stays selected; complete and new card both
 * count as a card. */
s32 title_check_card_slots(void) {
    s32 card_present = 0;
    s32 result;

    memcard_consume_all_events();

    memcard_set_selected_slot(1);
    result = memcard_poll_info_until_stable(3);
    if (result == MEMCARD_RESULT_COMPLETE || result == MEMCARD_RESULT_NEW_CARD) {
        card_present = 1;
    }

    memcard_set_selected_slot(0);
    result = memcard_poll_info_until_stable(3);
    if (result == MEMCARD_RESULT_COMPLETE || result == MEMCARD_RESULT_NEW_CARD) {
        card_present = 1;
    }

    return card_present;
}

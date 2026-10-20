/* Card info with retries, info result polling, polling until stable and slot checks. */

#include "fft/memcard.h"
#include "fft/title.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Issue _card_info on `port` up to retry_limit times, waiting for its BIOS event
 * each time. Returns complete on the first success, otherwise the result of the
 * last attempt (error when _card_info itself refused). */
memcard_result_e memcard_info_retry(s32 port, s32 retry_limit) {
    s32 i;
    memcard_result_e result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = memcard_bios_event_wait();
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
memcard_result_e memcard_operation_poll(void) {
    s32 status;
    memcard_result_e result;

    if (g_memcard_info_pending == 0) {
        _card_info(g_memcard_slot << 4);
        g_memcard_info_pending = 1;
    }

    status = _card_status(g_memcard_slot) & 1;
    result = memcard_bios_event_poll();
    if (status != 0 && result == MEMCARD_RESULT_PENDING) {
        g_memcard_info_pending = 0;
    }
    if (result >= MEMCARD_RESULT_COMPLETE) {
        g_memcard_info_pending = 0;
    }
    if (result > MEMCARD_RESULT_COMPLETE) {
        result = memcard_info_retry(g_memcard_slot << 4, 2);
    }
    if (result == MEMCARD_RESULT_ERROR) {
        result = MEMCARD_RESULT_TIMEOUT;
    }
    return result;
}

/* Probe the selected slot until three probes in a row complete, and return
 * complete. Any other settled result counts against retry_limit and is returned
 * when the limit is reached. A nonpositive limit leaves the result unset. */
static memcard_result_e memcard_poll_info_until_stable(s32 retry_limit) {
    s32 attempts = 0;
    s32 consecutive_successes = 0;
    memcard_result_e result;

    while (attempts < retry_limit) {
        do {
            result = memcard_operation_poll();
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
    memcard_result_e result;

    memcard_events_clear();

    memcard_slot_set(1);
    result = memcard_poll_info_until_stable(3);
    if (result == MEMCARD_RESULT_COMPLETE || result == MEMCARD_RESULT_NEW_CARD) {
        card_present = 1;
    }

    memcard_slot_set(0);
    result = memcard_poll_info_until_stable(3);
    if (result == MEMCARD_RESULT_COMPLETE || result == MEMCARD_RESULT_NEW_CARD) {
        card_present = 1;
    }

    return card_present;
}

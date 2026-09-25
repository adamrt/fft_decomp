#include "fft/open.h"

s32 open_card_poll_info_until_stable(s32 retry_limit) {
    s32 attempts = 0;
    s32 consecutive_successes = 0;
    s32 result;

    while (attempts < retry_limit) {
        do {
            result = open_card_poll_info_result();
        } while (result == CARD_IO_RESULT_PENDING);

        if (result == CARD_IO_RESULT_COMPLETE) {
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

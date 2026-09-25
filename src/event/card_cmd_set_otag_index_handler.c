#include "fft/event_card.h"
#include "psx/types.h"

const u8* card_cmd_set_otag_index_handler(const u8* data) {
    g_card_text_otag_index = data[3];
    return data + data[1];
}

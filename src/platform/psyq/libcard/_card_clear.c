#include "psx/libcard.h"

#define PSYQ_CARD_CLEAR_SYSTEM_SECTOR 0x3f

/* A dummy write to the system area clears the card's unconfirmed state. */
s32 _card_clear(s32 port) {
    _new_card();
    return _card_write(port, PSYQ_CARD_CLEAR_SYSTEM_SECTOR, 0);
}

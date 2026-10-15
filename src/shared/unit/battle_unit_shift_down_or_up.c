#include "fft/battle_unit.h"
#include "psx/types.h"

/* SEQ script step: moves the sprite offset by delta along the height axis;
 * positive is down. */
void battle_unit_shift_down_or_up(battle_unit_t* unit, s8 delta) {
    unit->seq_offset.vy += delta;
}

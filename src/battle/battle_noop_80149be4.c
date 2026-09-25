#include "fft/battle.h"

/* The scenario interpreter forwards its operand pointer in a0 even though
 * this entry does not consume it. Retain that caller-visible interface. */
void battle_noop_80149be4(const u8* unused_parameters) {
}

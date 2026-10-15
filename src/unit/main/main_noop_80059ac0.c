#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* An empty routine (jr ra; nop in the original) between
 * main_status_init_check_data and main_party_clear_all. No function in the tree
 * calls it. */
void main_noop_80059ac0(void) {
}

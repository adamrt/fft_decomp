#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* EVENT_OPCODE_UNKNOWN_74: receives the operand pointer and does nothing. */
void script_opcode_74_event_instruction(const u8* unused_parameters) {
}

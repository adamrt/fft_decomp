#include "fft/main.h"
#include "psx/types.h"

/* Sets the byte that SMD opcode 0x8D compares with its parameter. */
void main_smd_set_channel_select(suzuki_music_t* music, u8 value) {
    music->channel_select = value;
}

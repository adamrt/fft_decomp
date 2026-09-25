#include "fft/world.h"

void world_formation_show_unit_message(s16 unit_index, s32 message_id) {
    world_text_start_character_thread_if_idle(
        1, 0x1B, message_id, g_world_formation_unit_pointers[unit_index]->roster_slot);
}

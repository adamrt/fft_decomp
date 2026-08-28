#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void main_gfx_swap_and_clear_otag(void) {
    battle_render_buffer_t* battle_data = (battle_render_buffer_t*)g_battle_buffer_a;
    u32* otag;

    if (g_battle_data == battle_data) {
        battle_data++;
    }
    otag = battle_data->otag;
    g_battle_data = battle_data;
    ClearOTagR(otag, 0x180);
}

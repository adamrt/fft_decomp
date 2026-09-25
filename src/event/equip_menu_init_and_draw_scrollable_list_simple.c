#include "fft/event_equip.h"
#include "psx/types.h"

void equip_menu_init_and_draw_scrollable_list_simple(s32 a0, s32 a1, s32 a2, s32 g_main_input_secondary_repeat_period) {
    equip_menu_init_scrollable_list_core((s16*)a0, a1, (const void*)a2);
    equip_menu_draw_scrollable_list((u8*)g_main_input_secondary_repeat_period);
}

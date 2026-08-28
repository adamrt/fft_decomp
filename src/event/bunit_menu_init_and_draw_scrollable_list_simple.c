#include "fft/bunit.h"

void bunit_menu_init_and_draw_scrollable_list_simple(s32 a0, s32 a1, s32 a2, u8* script) {
    bunit_menu_init_scrollable_list_core((s16*)a0, a1, a2);
    bunit_menu_draw_scrollable_list(script);
}

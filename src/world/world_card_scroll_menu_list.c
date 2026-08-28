#include "fft/world.h"
#include "psx/types.h"

void world_card_scroll_menu_list(s32 row) {
    s16 velocity;
    s16 first;

    velocity = g_world_card_list_scroll_velocity;
    if (velocity == 0) {
        first = g_world_card_list_first_visible_row;
        if (row < first) {
            first -= 1;
            g_world_card_list_scroll_velocity = 4;
            g_world_card_list_first_visible_row = first;
        }
        first = g_world_card_list_first_visible_row;
        if (row >= first + 4) {
            first += 1;
            g_world_card_list_scroll_velocity = -4;
            g_world_card_list_first_visible_row = first;
        }
    } else if (velocity < 0) {
        g_world_card_list_scroll_velocity = velocity - 4;
        if (g_world_card_list_scroll_velocity < -0x2F) {
            g_world_card_list_scroll_velocity = 0;
            g_world_card_list_scroll_y -= 0x30;
        }
    } else if (velocity > 0) {
        g_world_card_list_scroll_velocity = velocity + 4;
        if (g_world_card_list_scroll_velocity >= 0x30) {
            g_world_card_list_scroll_velocity = 0;
            g_world_card_list_scroll_y += 0x30;
        }
    }
}

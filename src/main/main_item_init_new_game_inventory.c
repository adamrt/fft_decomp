#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void main_item_init_new_game_inventory(void) {
    int remaining = ITEM_ID_COUNT - 1;
    u8* quantity = &g_main_item_quantities[ITEM_ID_COUNT - 1];

    do {
        *quantity = 0;
        remaining--;
        quantity--;
    } while (remaining >= 0);

    g_main_item_quantities[ITEM_ID_POTION] = 5;
    g_main_item_quantities[ITEM_ID_HI_POTION] = 2;
    g_main_item_quantities[ITEM_ID_X_POTION] = 1;
    g_main_item_quantities[ITEM_ID_ETHER] = 1;
    g_main_item_quantities[ITEM_ID_HI_ETHER] = 1;
    g_main_item_quantities[ITEM_ID_ELIXIR] = 1;
    g_main_item_quantities[ITEM_ID_ANTIDOTE] = 1;
    g_main_item_quantities[ITEM_ID_EYE_DROP] = 2;
    g_main_item_quantities[ITEM_ID_ECHO_GRASS] = 1;
    g_main_item_quantities[ITEM_ID_MAIDENS_KISS] = 1;
    g_main_item_quantities[ITEM_ID_SOFT] = 1;
    g_main_item_quantities[ITEM_ID_HOLY_WATER] = 1;
    g_main_item_quantities[ITEM_ID_REMEDY] = 1;
    g_main_item_quantities[ITEM_ID_PHOENIX_DOWN] = 2;
}

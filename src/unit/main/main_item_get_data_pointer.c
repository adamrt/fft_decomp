#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

item_data_t* main_item_get_data_pointer(s32 item_id) {
    return &g_main_item_primary_data[item_id & ITEM_ID_MASK];
}

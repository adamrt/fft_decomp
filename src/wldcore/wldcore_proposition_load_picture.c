#include "fft/wldcore.h"
#include "psx/types.h"

s32 wldcore_proposition_load_picture(s32 index) {
    s32 value = g_wldcore_location_records[index].picture;

    if (value != 0)
        wldcore_proposition_load_picture_data_set(value - 1);
    return value;
}

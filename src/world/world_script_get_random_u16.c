#include "fft/event.h"
#include "fft/world.h"

u32 world_script_get_random_u16(void) {
    u32 seed = g_world_event_random_seed;
    u32 triple = (seed << 1) + seed;
    u32 fifty_one = triple + (triple << 4);
    u32 two_hundred_three = (fifty_one << 2) - seed;
    u32 six_thousand_two_hundred_ninety_three = (two_hundred_three << 5) - two_hundred_three;
    u32 result = (six_thousand_two_hundred_ninety_three << 2) + seed + 0x3619;

    g_world_event_random_seed = result & 0xffff;
    return g_world_event_random_seed;
}

#include "fft/battle.h"

s32 battle_effect_get_random_between(s32 value_a, s32 value_b) {
    s32 first;
    s32 second;
    s32 remainder;
    s32 random_value;
    s32 result;

    first = value_a;
    second = value_b;
    if (first == second) {
        return first;
    }
    if (second < first) {
        random_value = rand();
        remainder = random_value % (first - second);
        result = remainder + second;
    } else {
        random_value = rand();
        remainder = random_value % (second - first);
        result = remainder + first;
    }
    return result;
}

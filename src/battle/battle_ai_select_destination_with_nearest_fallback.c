#include "fft/battle_ai.h"

/* Prefer priority within range, falling back to the nearest reachable tile.
 *
 * The fallback removes the distance bound. Neither pass requires a positive
 * priority. */
void battle_ai_select_destination_with_nearest_fallback(s32 range) {
    if (battle_ai_select_destination(range, BATTLE_AI_DESTINATION_PRIORITY_FIRST) == 0) {
        battle_ai_select_destination(0x7fffffff, BATTLE_AI_DESTINATION_DISTANCE_FIRST);
    }
}
